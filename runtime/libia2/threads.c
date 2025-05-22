#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <sys/mman.h>

__attribute__((visibility("default"))) void init_stacks_and_setup_tls(void);
__attribute__((visibility("default"))) void **ia2_stackptr_for_tag(size_t tag);

struct ia2_thread_thunk {
  void *(*fn)(void *);
  void *data;
};

size_t ia2_get_tag(void);

/* __thread char ia2_signal_stack[STACK_SIZE]; */

void *ia2_thread_begin(void *arg) {
  struct ia2_thread_thunk *thunk = (struct ia2_thread_thunk *)arg;
  void *(*fn)(void *) = thunk->fn;
  void *data = thunk->data;
#if IA2_DEBUG_LOG
  printf("%s: creating thread with function at %p and argument %p\n", __func__, fn, data);
#endif
  /* stack_t alt_stack = { */
  /*     .ss_sp = ia2_signal_stack, .ss_flags = 0, .ss_size = STACK_SIZE}; */

  /* Free the thunk. */
  munmap(arg, sizeof(struct ia2_thread_thunk));

  init_stacks_and_setup_tls();
  /* TODO: Set up alternate stack when we have per-thread shared compartment
   * data. */
  /*  sigaltstack(&alt_stack, NULL); */

  /* Determine the current compartment so know which stack to use. */
  size_t tag = ia2_get_tag();
  void **new_sp_addr = ia2_stackptr_for_tag(tag);

  /* Switch to the stack for this compartment, then call `fn(data)`. */
  void *result;
#if defined(__x86_64__)
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
#elif defined(__aarch64__)
  __asm__ volatile(
        // Load argument
        "ldr x0, [%[data]]\n"
        // Copy stack pointer to x10
        "mov x10, sp\n"
        // Load the stack pointer for this compartment's stack
        "ldr x0, [%[new_sp_addr]]\n"
        "mov sp, x0\n"
        // Push the old stack pointer
        "str x10, [sp, #-8]!\n"
        // Align the stack
        "movn x10, #0x000f\n"
        "mov x11, sp\n"
        "and x11, x11, x10\n"
        "mov sp, x11\n"
        // Prologue
        "str x29, [sp, #-8]!\n"
        "mov x29, sp\n"
        // Call fn(data)
        "blr %[fn]\n"
        // Pop the old stack pointer
        "ldr x10, [sp], #8\n"
        // Switch stacks back
        "mov sp, x10\n"
        // x0 now contains ret value
        "mov %[result], x0\n"
      : [result] "=r"(result)
      : [fn] "r"(fn), [data] "r"(data), [new_sp_addr] "r"(new_sp_addr)
      : "x0", "x10", "x11");
#else
#error "unknown architecture"
#endif
  /* clang-format on */

  return result;
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
