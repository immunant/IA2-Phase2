#ifndef _GNU_SOURCE
#define _GNU_SOURCE // for SIG* macro
#endif
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>

/*
 * This header implements an signal handler to allow running compartmentalized
 * programs in a "permissive mode" which logs cross-compartment memory accesses
 * instead of terminating the process. To use it, just #include this header from
 * one translation unit in a program's main binary. Permissive mode is still
 * experimental and is not routinely used in the test suite in this repo.
 *
 * This works by installing a signal handler for SIGSEGV and SIGTRAP from a
 * constructor. This constructor also creates a new thread for logging memory
 * access violations and reruns itself whenever a process forks. When a
 * cross-compartment memory access triggers a segfault, the handler saves the
 * PKRU from the ucontext argument and sets it to 0 (allows access to all
 * compartments) for when the handler returns. The handler then sets the TRAP
 * bit in the EFLAGS register through the ucontext to single-step through code
 * when the handler returns. Finally this handler pushes the address that caused
 * the segfault and the program counter value onto a fixed-sized queue protected
 * by a spinlock.
 *
 * When the handler returns from the first attempt at a cross-compartment memory
 * access, it will successfully re-execute the instruction that caused the
 * segfault with PKRU 0 then a SIGTRAP will be raised since the TRAP bit in
 * EFLAGS is set. The SIGTRAP handler then restores the PKRU to its old value
 * and clears the TRAP bit in EFLAGS through the ucontext. When this handler
 * returns the program continues execution normally starting from the
 * instruction after the one that caused the cross-compartment memory access.
 *
 * Since signal handlers are limited to calling async-signal-safe functions, a
 * separate thread is used to log cross-compartment memory accesses. This thread
 * pops entries from the queue mentioned above then uses the dladdr to try to
 * get symbol information for both the memory address and the program counter.
 * Since this logging happens asynchronously, this header also defines a
 * destructor that notifies the logging thread that the process is exiting and
 * waits until it pops everything from the queue. Finally it prints the contents
 * of /proc/self/maps at the end of each log. Note that it only prints the
 * memory map when exiting so the addresses logged might not always correspond
 * to the segments in the map, but this is unlikely to be an issue unless
 * shared objects are repeatedly loaded and unloaded in the same range of
 * addresses.
 */

/*
 * Function-like macros are not rewritten so use this macro to avoid changing
 * some function pointers below without adding another explicit exception in the
 * rewriter.
 */
#define IA2_IGNORE(x) x

int pkru_offset(void) {
  unsigned int eax = 0xd;
  unsigned int ecx = 9;
  unsigned int ebx, edx;
  asm volatile("cpuid"
               : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
               : "0"(eax), "2"(ebx));
  return ebx;
}

/*
 * Use an arbitrary fixed queue size since push happens in the signal handler
 * and we can't realloc more memory there since it's not async-signal-safe. We
 * could use dynamic allocation by periodically increasing the capacity when the
 * logging thread pops but there's not much benefit to this for debugging.
 */
#define QUEUE_SIZE 1024

// The entries in the queue
typedef struct mpk_err {
  uint64_t addr;
  uint64_t pc;
} mpk_err;

struct queue {
  int locked;
  size_t push;
  size_t pop;
  mpk_err data[QUEUE_SIZE];
};

/*
 * The queue is modified by the logging thread and signal handler so protect it
 * with a spinlock.
 */
struct queue *get_queue(void) {
  static struct queue mpk_violations = {0};
  while (!__sync_bool_compare_and_swap(&mpk_violations.locked, 0, 1))
    ;
  return &mpk_violations;
}

void release_queue(struct queue *q) {
  __asm__ volatile("" ::: "memory");
  q->locked = 0;
}

void push_queue(struct queue *q, mpk_err e) {
  assert(q);
  assert(q->push < QUEUE_SIZE);
  q->data[q->push] = e;
  q->push += 1;
}

// Attempts to pop an entry from the queue and returns if it succeeded or not.
bool pop_queue(struct queue *q, mpk_err *e) {
  assert(q);
  assert(q->pop < QUEUE_SIZE);
  assert(e);
  if (q->push == q->pop) {
    return false;
  } else {
    *e = q->data[q->pop];
    q->pop += 1;
    return true;
  }
}

// The main signal handler
void permissive_mode_handler(int sig, siginfo_t *info, void *ctxt) {
  bool handling_pkuerr = sig == SIGSEGV && info && info->si_code == SEGV_PKUERR;
  bool handling_trap = sig == SIGTRAP;
  if (!handling_pkuerr && !handling_trap) {
    return;
  }
  ucontext_t *uctxt = (ucontext_t *)ctxt;
  uint64_t *eflags = (uint64_t *)(&uctxt->uc_mcontext.gregs[REG_EFL]);

  // Calculate the offset of the PKRU within fpregs in ucontext
  int offset = pkru_offset();
  char *fpregs = (char *)uctxt->uc_mcontext.fpregs;
  uint32_t *pkru = (uint32_t *)(&fpregs[offset]);

  const uint64_t trap_bit = 0x100;
  static __thread uint32_t old_pkru = 0;
  if (handling_pkuerr) {
    // Set the EFLAGS trap bit when the handler returns
    *eflags |= trap_bit;
    // Save the PKRU in the ucontext
    old_pkru = *pkru;
    // Set the PKRU for when the handler returns
    *pkru = 0;

    // Push the memory address and progam counter onto the queue
    uint64_t pc = (uint64_t)uctxt->uc_mcontext.gregs[REG_RIP];
    // Ensure that the logging thread is not popping values
    struct queue *q = get_queue();
    mpk_err err = {.addr = (uint64_t)info->si_addr, .pc = pc};
    push_queue(q, err);
    release_queue(q);

  } else if (handling_trap) {
    // Restore the PKRU for when the handler returns
    *pkru = old_pkru;
    // Clear the EFLAGS trap bit when the handler returns
    *eflags &= ~trap_bit;
  }
}

// Flag to notify logging thread that the process is exiting
static bool exiting = false;

// Process-specific name for log file
static char log_name[24] = {0};

// The main function in the logging thread
void *log_mpk_violations(void *arg) {
  snprintf(log_name, sizeof(log_name), "mpk_log_%d", getpid());
  FILE *log = fopen(log_name, "w");
  assert(log);
  while (1) {
    // We don't want this thread to just spin if nothing is happening
    sleep(1);

    // Ensure the sighandler isn't pushing onto the queue
    struct queue *q = get_queue();
    mpk_err err;
    // Pop everything currently in the queue
    while (pop_queue(q, &err)) {
      const char *unknown = "<unknown>";
      Dl_info dlinf = {0};
      dladdr((void *)err.addr, &dlinf);
      const char *sym_name = dlinf.dli_sname;
      if (!sym_name) {
        sym_name = unknown;
      }
      dladdr((void *)err.pc, &dlinf);
      const char *fn_name = dlinf.dli_sname;
      if (!fn_name) {
        fn_name = unknown;
      }
      fprintf(log, "MPK error accessing %s (%lx) from %s (%lx)\n", sym_name,
              err.addr, fn_name, err.pc);
    }
    release_queue(q);

    if (exiting) {
      fclose(log);
      return NULL;
    }
  }
}

/*
 * Linux sets the PKRU to only allow access to the untrusted compartment in
 * signal handlers and it may be executed from any compartment (and their
 * corresponding stack). This means that the handler registered with sigaction
 * must be a naked function to ensure the stack is not accessed before zeroing
 * out the PKRU. The PKRU is then restored by the kernel when returning.
 */
__attribute__((naked)) void permissive_mode_trampoline(int sig, siginfo_t *info,
                                                       void *ctxt) {
  __asm__(
      // rcx and rdx may arguments so preserve them across the wrpkru
      "movq %rcx, %r10\n"
      "movq %rdx, %r11\n"
      // zero the PKRU to allow access to all compartments
      "xorl %ecx, %ecx\n"
      "xorl %edx, %edx\n"
      "xorl %eax, %eax\n"
      "wrpkru\n"
      "movq %r10, %rcx\n"
      "movq %r11, %rdx\n"
      "jmp permissive_mode_handler");
}

static pthread_t logging_thread;

/*
 * The logging thread is created from a constructor which happens before the
 * call gate wrapper for `main` so we need to call the real pthread_create, not
 * the compartment-aware wrapper for it. The following line just declares
 * __real_pthread_create (symbol created by ld becaused of the
 * --wrap=pthread_create flag) with the same function signature as
 * pthread_create.
 */
typeof(IA2_IGNORE(pthread_create)) __real_pthread_create;

// The constructor that installs the signal handlers
__attribute__((constructor)) void install_permissive_mode_handler(void) {
  static struct sigaction act = {
      .sa_sigaction = IA2_IGNORE(permissive_mode_trampoline),
      .sa_flags = SA_SIGINFO,
  };
  sigemptyset(&(act.sa_mask));
  // mask SIGTRAP in SIGSEGV handler
  sigaddset(&(act.sa_mask), SIGTRAP);
  sigaction(SIGSEGV, &act, NULL);

  static struct sigaction act2 = {
      .sa_sigaction = IA2_IGNORE(permissive_mode_trampoline),
      .sa_flags = SA_SIGINFO,
  };
  sigemptyset(&(act2.sa_mask));
  sigaction(SIGTRAP, &act2, NULL);

  // Create the logging thread
  __real_pthread_create(&logging_thread, NULL, IA2_IGNORE(log_mpk_violations),
                        NULL);
  // When the process forks call this function again to reinstall these signal
  // handlers and call pthread_atfork again
  pthread_atfork(NULL, NULL, IA2_IGNORE(install_permissive_mode_handler));
}

/*
 * Constructor to wait for the logging thread to finish and log the memory map.
 * If a process forks and execve's this function will not be called.
 */
__attribute((destructor)) void wait_logging_thread(void) {
  exiting = true;
  pthread_join(logging_thread, NULL);
  FILE *log = fopen(log_name, "a");
  assert(log);
  FILE *maps = fopen("/proc/self/maps", "r");
  assert(maps);
  char tmp[256] = {0};
  while (fgets(tmp, sizeof(tmp), maps)) {
    fprintf(log, "%s", tmp);
    memset(tmp, 0, sizeof(tmp));
  }
  fclose(log);
  fclose(maps);
}
