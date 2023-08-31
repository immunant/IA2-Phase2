#if IA2_ENABLE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE // for SIG* macro
#endif
#include <assert.h>
#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ucontext.h>

#include "ia2.h"

/*
 * PKRU is defined to be bit 9 in the XSAVE state component bitmap (Intel SDM
 * Section 13.1).
 */
#define XFEATURE_PKRU 9
#define XFEATURE_MASK_PKRU		(1 << XFEATURE_PKRU)

/*
 * Offset of the XSAVE header in bytes from the XSAVE area base (Intel SDM
 * Section 13.4.2)
 */
#define XSAVE_HEADER_OFFSET 512

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

int pkru_offset(void) {
  unsigned int eax = 0xd;
  unsigned int ecx = 9;
  unsigned int ebx, edx;
  asm volatile("cpuid"
               : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
               : "0"(eax), "2"(ecx));
  return ebx;
}

/*
 * Use an arbitrary fixed queue size since push happens in the signal handler
 * and we can't realloc more memory there since it's not async-signal-safe. We
 * could use dynamic allocation by periodically increasing the capacity when the
 * logging thread pops but there's not much benefit to this for debugging.
 */
#define QUEUE_SIZE 8192

// The entries in the queue
typedef struct mpk_err {
  uint64_t addr;
  uint64_t val;
  uint64_t pc;
  uint64_t sp;
  uint64_t fp;
  uint32_t pkru;
  uint64_t local_addr;
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
  static struct queue mpk_violations IA2_SHARED_DATA = {0};
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
  if (q->push == QUEUE_SIZE) {
    q->push = 0;
  }
  // Check if the queue is full
  assert(q->push + 1 != q->pop);
  q->data[q->push] = e;
  q->push += 1;
}

// Attempts to pop an entry from the queue and returns if it succeeded or not.
bool pop_queue(struct queue *q, mpk_err *e) {
  assert(q);
  assert(e);
  if (q->pop == QUEUE_SIZE) {
    q->pop = 0;
  }
  if (q->push == q->pop) {
    // Queue is empty
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
  ucontext_t *uctxt = (ucontext_t *)ctxt;
  if (!handling_pkuerr && !handling_trap) {
    abort();
  }
  uint64_t *eflags = (uint64_t *)(&uctxt->uc_mcontext.gregs[REG_EFL]);

  // Calculate the offset of the PKRU within fpregs in ucontext
  int offset = pkru_offset();
  char *fpregs = (char *)uctxt->uc_mcontext.fpregs;
  uint32_t *pkru = (uint32_t *)(&fpregs[offset]);

  // Set bit 9 (PKRU) in the xfeatures so that the PKRU register always gets
  // restored from the XSAVE state after the signal handler. See
  // copy_uabi_to_xstate() in the kernel xstate handling.
  uint64_t *xsave_header = (uint64_t *)(&fpregs[XSAVE_HEADER_OFFSET]);
  *xsave_header |= XFEATURE_MASK_PKRU;

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
    uint64_t sp = (uint64_t)uctxt->uc_mcontext.gregs[REG_RSP];
    uint64_t fp = (uint64_t)uctxt->uc_mcontext.gregs[REG_RBP];
    // Ensure that the logging thread is not popping values
    struct queue *q = get_queue();
    uint64_t val = *(uint64_t *)info->si_addr;
    mpk_err err = {.addr = (uint64_t)info->si_addr, .val = val, .pc = pc, .sp = sp, .fp = fp, .pkru = old_pkru, .local_addr = (uint64_t)&pc};
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
static bool exiting IA2_SHARED_DATA = false;

// Process-specific name for log file
static char log_name[256] IA2_SHARED_DATA = {0};

int elfaddr(const void *addr, Dl_info *info) {
  static struct f {
    struct f *f_next;
    const char *f_path;
    Elf64_Sym *f_symtab;
    char *f_strtab;
    size_t f_symcnt;
  } *head;
  struct f *fp;
  Elf64_Sym *best;
  const char *path;
  int i;

  if (dladdr(addr, info) == 0) {
    // printf("Failed to map address %lx to a shared object\n", addr);
    return 0;
  }
  path = info->dli_fname;

  // Did we already open this file?
  for (fp = head; fp != NULL; fp = head->f_next) {
    if (strcmp(path, fp->f_path) == 0)
      break;
  }
  if (fp == NULL) {
    Elf64_Ehdr *header;
    Elf64_Shdr *sections;
    Elf64_Sym *symtab;
    size_t len, symcnt;
    int fd;
    char *strtab;

    fp = calloc(1, sizeof(*fp));
    if (fp == NULL) {
      printf("Failed to allocate memory\n");
      return 0;
    }
    fp->f_next = head;
    fp->f_path = path;
    head = fp;

    // Read the ELF header.
    fd = open(path, O_RDONLY);
    if (fd < 0) {
      printf("Failed to open file %s\n", path);
      return 0;
    }
    header = mmap(NULL, sizeof(*header), PROT_READ, MAP_PRIVATE, fd, 0);
    if (header == MAP_FAILED || memcmp(header->e_ident, ELFMAG, SELFMAG) != 0 ||
        header->e_ident[EI_CLASS] != ELFCLASS64) {
      printf("Bad ELF magic or class\n");
      return 0;
    }

    // Read the section headers.
    len = header->e_shnum * sizeof(sections[0]);
    if (len / sizeof(sections[0]) < header->e_shnum) {
      printf("Invalid ELF section header size\n");
      return 0;
    }
    off_t off = (header->e_shoff) - (header->e_shoff % 4096);
    len += (header->e_shoff % 4096);
    char *sections_tmp = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, off);
    if (sections_tmp == MAP_FAILED) {
      printf("Failed to mmap ELF section header %s %zu %ld\n", strerror(errno),
             len, off);
      return 0;
    }
    sections_tmp += header->e_shoff % 4096;
    sections = (void *)sections_tmp;

    // Find and read the .symtab section.
    for (i = 0; i < header->e_shnum; i++) {
      if (sections[i].sh_type == SHT_SYMTAB)
        break;
    }
    if (i == header->e_shnum) {
      printf("Failed to find ELF .symtab section in %s ELF header\n", path);
      return 0;
    }
    off = sections[i].sh_offset - (sections[i].sh_offset % 4096);
    len = sections[i].sh_size + (sections[i].sh_offset % 4096);
    char *symtab_tmp = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, off);
    if (symtab_tmp == MAP_FAILED) {
      printf("Failed to find ELF .symtab section in %s\n", path);
      return 0;
    }
    symtab = (void *)(symtab_tmp + sections[i].sh_offset % 4096);
    symcnt = sections[i].sh_size / sizeof(symtab[0]);

    // Find and read the .strtab section.
    i = sections[i].sh_link;
    strtab = mmap(NULL, sections[i].sh_size, PROT_READ, MAP_PRIVATE, fd,
                  sections[i].sh_offset);
    if (strtab == MAP_FAILED)
      return 0;

    fp->f_symtab = symtab;
    fp->f_strtab = strtab;
    fp->f_symcnt = symcnt;
    printf("Found %zu symbols\n", fp->f_symcnt);
  }

  best = NULL;
  for (i = 0; i < fp->f_symcnt; i++) {
    Elf64_Sym *sym = &fp->f_symtab[i];
    if (fp->f_strtab[sym->st_name] == '\0')
      continue;
    if (sym->st_value <= ((Elf64_Addr)addr - (Elf64_Addr)info->dli_fbase) &&
        (best == NULL || best->st_value < sym->st_value))
      best = sym;
  }
  if (best == NULL) {
    return 0;
  }
  info->dli_sname = &fp->f_strtab[best->st_name];
  info->dli_saddr = (void *)((Elf64_Addr)info->dli_fbase + best->st_value);
  return 1;
}

// Print an address, including relative offset if loaded from disk and symbol
// name if available.
void print_address(FILE *log, char *identifier, void *addr) {
  Dl_info dlinf = {0};
  bool found = dladdr(addr, &dlinf) != 0;

  if (found) {
    uintptr_t offset = (uintptr_t)addr - (uintptr_t)dlinf.dli_fbase;
    const char *fname = basename(dlinf.dli_fname);

    if (dlinf.dli_sname) {
      uintptr_t offset = (uintptr_t)addr - (uintptr_t)dlinf.dli_saddr;
      fprintf(log, "%s: %s:%s+0x%" PRIxPTR " [%p] ", identifier, fname,
              dlinf.dli_sname, offset, addr);
    } else {
      fprintf(log, "%s: %s+0x%" PRIxPTR " [%p], ", identifier, fname, offset,
              addr);
    }
  } else {
      fprintf(log, "%s: %p, ", identifier, addr);
  }
}

// The main function in the logging thread
void *log_mpk_violations(void *arg) {
  /* Explicitly enter compartment 1, because this function isn't wrapped. */
  __asm__("wrpkru\n" : : "a"(0xFFFFFFF0), "d"(0), "c"(0));
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
      print_address(log, "addr", (void *)err.addr);
      print_address(log, "val", (void *)err.val);
      print_address(log, "pc", (void *)err.pc);
      print_address(log, "sp", (void *)err.sp);
      print_address(log, "fp", (void *)err.fp);
      fprintf(log, "pkru: %x\n", err.pkru);
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
      "movq %rax, %r12\n"
      // zero the PKRU to allow access to all compartments
      "xorl %ecx, %ecx\n"
      "xorl %edx, %edx\n"
      "xorl %eax, %eax\n"
      "wrpkru\n"
      "movq %r10, %rcx\n"
      "movq %r11, %rdx\n"
      "movq %r12, %rax\n"
      "jmp permissive_mode_handler");
}

static pthread_t logging_thread IA2_SHARED_DATA;

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
  struct sigaction act = {
      .sa_sigaction = IA2_IGNORE(permissive_mode_trampoline),
      .sa_flags = SA_SIGINFO,
  };
  sigemptyset(&(act.sa_mask));
  // mask SIGTRAP in SIGSEGV handler
  sigaddset(&(act.sa_mask), SIGTRAP);
  sigaction(SIGSEGV, &act, NULL);

  struct sigaction act2 = {
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
#endif // IA2_ENABLE
