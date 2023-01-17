#include "library.h"
#include <assert.h>
#include <ia2.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

INIT_RUNTIME(1);
INIT_COMPARTMENT(1);

int data_in_main = 30;

void defined_in_main(void) { printf("main data is %d\n", data_in_main); }

IA2_DEFINE_WRAPPER(defined_in_main, _ZTSPFvvE, 1);

Fn fnptr_from_main = IA2_WRAPPER(defined_in_main, 1);

static inline unsigned int rdpkru(void) {
  uint32_t pkru = 0;
  __asm__ volatile(IA2_RDPKRU : "=a"(pkru) : "a"(0), "d"(0), "c"(0));
  return pkru;
}

void *thread_fn(void *ptr);

void *thread_fn(void *ptr) {
  printf("tid %d ptr=%p\n", gettid(), ptr);

  printf("main-module thread pkru=%08x\n", rdpkru());

  library_showpkru();

  printf("ptr is %p, formatting int: %08x\n", ptr, 4300 + ptr);

  return NULL;
}
pthread_t thread1, thread2;

void create_threads(void) {
  int iret1, iret2;

  iret1 = pthread_create(&thread1, NULL, thread_fn, (void *)1);
  iret2 = pthread_create(&thread2, NULL, thread_fn, (void *)2);
}

int main() {
  printf("main-module main pkru=%08x\n", rdpkru());
  library_showpkru();
  printf("main-module main pkru=%08x\n", rdpkru());

  pthread_t lib_thread = library_spawn_thread();

  create_threads();
  library_foo();
  defined_in_main();
  library_call_fn(fnptr_from_main);

  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);
  pthread_join(lib_thread, NULL);
  return 0;
}
