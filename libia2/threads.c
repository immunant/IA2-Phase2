#include <pthread.h>

#include "ia2.h"

void init_stacks(void);
void protect_tls(void);
void **ia2_stackptr_for_pkru(uint32_t pkey);

struct ia2_thread_thunk {
  void *(*fn)(void *);
  void *data;
};

void *ia2_thread_begin(void *arg) {
  struct ia2_thread_thunk *thunk = (struct ia2_thread_thunk *)arg;
  void *(*fn)(void *) = thunk->fn;
  void *data = thunk->data;
  /* Free the thunk. */
  munmap(arg, sizeof(struct ia2_thread_thunk));

  protect_tls();
  init_stacks();

  /* Determine the current compartment so know which stack to use. */
  uint32_t pkru = 0;
  __asm__ volatile(
      /* clang-format off */
      "xor %%ecx,%%ecx\n"
      IA2_RDPKRU "\n"
      /* clang-format on */
      : "=a"(pkru)::"ecx", "edx");
  /* returns NULL in insecure mode, because we can't read a pkru there. */
  void **new_sp_addr = ia2_stackptr_for_pkru(pkru);

  /* Switch to the stack for this compartment, then call `fn(data)`. */
  void *result;
  __asm__ volatile(
      /* clang-format off */
      // Copy stack pointer to rdi.
      "movq %%rsp, %%rdi\n"
      // In insecure mode, we can't read pkru to know which compartment the
      // thread started in (inherited from the parent thread), so we don't know
      // which stack it would be appropriate to switch to here.
#ifndef LIBIA2_INSECURE
      // Load the stack pointer for this compartment's stack.
      "mov (%[new_sp_addr]), %%rsp\n"
#endif
      // Push old stack pointer, which aligns the stack.
      "pushq %%rdi\n"
      "pushq %%rbp\n"
      // Call fn(data).
      "mov %[data], %%rdi\n"
      "call *%[fn]\n"
      // Restore old stack pointer.
      "popq %%rbp\n"
      "popq %%rsp\n"
      : "=a"(result)
      : [fn] "r"(fn), [data] "r"(data), [new_sp_addr] "r"(new_sp_addr)
      : "rdi");
  /* clang-format on */
  return result;
}

int __real_pthread_create(pthread_t *restrict thread,
                          const pthread_attr_t *restrict attr, void *(*fn)(),
                          void *data);

int __wrap_pthread_create(pthread_t *restrict thread,
                          const pthread_attr_t *restrict attr, void *(*fn)(),
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
