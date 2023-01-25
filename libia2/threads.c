#include <pthread.h>

#include "ia2.h"

void init_stacks(void);

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

  init_stacks();

  /* TODO: switch to compartment stack before calling `fn` */

  return fn(data);
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
