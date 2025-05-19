#include "ia2_threads.h"

#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>

__attribute__((visibility("default"))) void init_stacks_and_setup_tls(void);
__attribute__((visibility("default"))) void **ia2_stackptr_for_pkru(uint32_t pkey);

struct ia2_thread_thunk {
  void *(*fn)(void *);
  void *data;
};

/* __thread char ia2_signal_stack[STACK_SIZE]; */

void *ia2_thread_begin(void *arg) {
  struct ia2_thread_thunk *thunk = (struct ia2_thread_thunk *)arg;
  void *(*fn)(void *) = thunk->fn;
  void *data = thunk->data;
  /* stack_t alt_stack = { */
  /*     .ss_sp = ia2_signal_stack, .ss_flags = 0, .ss_size = STACK_SIZE}; */

  /* Free the thunk. */
  munmap(arg, sizeof(struct ia2_thread_thunk));

  init_stacks_and_setup_tls();
  /* TODO: Set up alternate stack when we have per-thread shared compartment
   * data. */
  /*  sigaltstack(&alt_stack, NULL); */

#if defined(__x86_64__)
  /* Determine the current compartment so know which stack to use. */
  uint32_t pkru = 0;
  __asm__ volatile(
      /* clang-format off */
      "xor %%ecx,%%ecx\n"
      "rdpkru\n"
      /* clang-format on */
      : "=a"(pkru)::"ecx", "edx");
  void **new_sp_addr = ia2_stackptr_for_pkru(pkru);

  /* Switch to the stack for this compartment, then call `fn(data)`. */
  void *result;
  __asm__ volatile(
      /* clang-format off */
      // Copy stack pointer to rdi.
      "movq %%rsp, %%rdi\n"
      // Load the stack pointer for this compartment's stack.
      "mov (%[new_sp_addr]), %%rsp\n"
      // Push old stack pointer.
      "pushq %%rdi\n"
      "pushq %%rbp\n"
      // Load argument.
      "mov %[data], %%rdi\n"
      // Save %rsp before alignment.
      "movq %%rsp, %%rbp\n"
      // Align the stack.
      "andq $0xfffffffffffffff0, %%rsp\n"
      // Call fn(data).
      "call *%[fn]\n"
      // Restore pre-alignment stack pointer.
      "movq %%rbp, %%rsp\n"
      // Switch stacks back.
      "popq %%rbp\n"
      "popq %%rsp\n"
      : "=a"(result)
      : [fn] "r"(fn), [data] "r"(data), [new_sp_addr] "r"(new_sp_addr)
      : "rdi");
  /* clang-format on */
  return result;
#elif defined(__aarch64__)
#warning "libia2 does not implement ia2_thread_begin yet"
  __builtin_trap();
#endif
}

typeof(pthread_create) __real_pthread_create;

int __wrap_pthread_create(pthread_t *restrict thread,
                          const pthread_attr_t *restrict attr, void *(*fn)(void *),
                          void *data) {
  /* Allocate a thunk for the thread to call `ia2_thread_begin` before the
  provided thread body function. We cannot use malloc()/free() here because the
  newly-started thread needs to free the allocation that this thread makes,
  which is not permitted by all allocators (we see segfaults). */
  void *mmap_res = mmap(NULL, sizeof(struct ia2_thread_thunk),
                        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (mmap_res == MAP_FAILED) {
    return -1;
  }
  struct ia2_thread_thunk *thread_thunk = (struct ia2_thread_thunk *)mmap_res;
  thread_thunk->fn = fn;
  thread_thunk->data = data;
  return __real_pthread_create(thread, attr, ia2_thread_begin, thread_thunk);
}

// Only enable this code that stores these addresses when debug logging is enabled.
// This reduces the trusted codebase and avoids runtime overhead.
#if IA2_DEBUG_LOG

struct ia2_all_threads_metadata {
  pthread_mutex_t lock;
  size_t num_threads;
  pid_t tids[IA2_MAX_THREADS];
  struct ia2_thread_metadata thread_metadata[IA2_MAX_THREADS];
};

#define array_len(a) (sizeof(a) / sizeof(*(a)))

struct ia2_thread_metadata *ia2_all_threads_metadata_lookup(struct ia2_all_threads_metadata *const this) {
  const pid_t tid = gettid();

  struct ia2_thread_metadata *metadata = NULL;
  if (pthread_mutex_lock(&this->lock) != 0) {
    perror("pthread_mutex_lock in ia2_all_threads_data_lookup failed");
    goto ret;
  }
  for (size_t i = 0; i < this->num_threads; i++) {
    if (this->tids[i] == tid) {
      metadata = &this->thread_metadata[i];
      goto unlock;
    }
  }
  if (this->num_threads >= array_len(this->thread_metadata)) {
    fprintf(stderr, "created %zu threads, but can't store them all (max is IA2_MAX_THREADS)\n", this->num_threads);
    goto unlock;
  }

  metadata = &this->thread_metadata[this->num_threads];
  this->tids[this->num_threads] = tid;
  this->num_threads++;

  metadata->tid = tid;
  metadata->thread = pthread_self();

  goto unlock;

unlock:
  if (pthread_mutex_unlock(&this->lock) != 0) {
    perror("pthread_mutex_unlock in ia2_all_threads_data_lookup failed");
  }
ret:
  return metadata;
}

struct ia2_addr_location ia2_all_threads_metadata_find_addr(struct ia2_all_threads_metadata *const this, const uintptr_t addr) {
  struct ia2_addr_location location = {
      .name = NULL,
      .tid = -1,
      .compartment = -1,
  };
  if (pthread_mutex_lock(&this->lock) != 0) {
    perror("pthread_mutex_lock in ia2_all_threads_data_find_addr failed");
    goto ret;
  }
  for (size_t thread = 0; thread < this->num_threads; thread++) {
    const pid_t tid = this->tids[thread];
    for (int compartment = 0; compartment < IA2_MAX_COMPARTMENTS; compartment++) {
      const struct ia2_thread_metadata *const thread_metadata = &this->thread_metadata[thread];
      if (addr == thread_metadata->stack_addrs[compartment]) {
        location.name = "stack";
        location.tid = tid;
        location.thread = thread_metadata->thread;
        location.compartment = compartment;
        goto unlock;
      }
      if (addr == thread_metadata->tls_addrs[compartment]) {
        location.name = "tls";
        location.tid = tid;
        location.thread = thread_metadata->thread;
        location.compartment = compartment;
        goto unlock;
      }
      if (addr == thread_metadata->tls_addr_compartment1_first || addr == thread_metadata->tls_addr_compartment1_second) {
        location.name = "tls";
        location.tid = tid;
        location.thread = thread_metadata->thread;
        location.compartment = 1;
        goto unlock;
      }
    }
  }

  goto unlock;

unlock:
  if (pthread_mutex_unlock(&this->lock) != 0) {
    perror("pthread_mutex_unlock in ia2_all_threads_data_find_addr failed");
  }
ret:
  return location;
}

// All zeroed, so this should go in `.bss` and only have pages lazily allocated.
static struct ia2_all_threads_metadata IA2_SHARED_DATA threads = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .num_threads = 0,
    .thread_metadata = {0},
};


struct ia2_thread_metadata *ia2_thread_metadata_get_current_thread(void) {
  return ia2_all_threads_metadata_lookup(&threads);
}

#endif // IA2_DEBUG_LOG

struct ia2_addr_location ia2_addr_location_find(const uintptr_t addr) {
#if IA2_DEBUG_LOG
  return ia2_all_threads_metadata_find_addr(&threads, addr);
#else
  return (struct ia2_addr_location){
      .name = NULL,
      .tid = -1,
      .compartment = -1,
  };
#endif
}
