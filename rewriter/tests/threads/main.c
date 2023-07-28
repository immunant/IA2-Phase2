/*
RUN: sh -c 'if [ ! -s "threads_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
RUN: %binary_dir/tests/threads/threads_main_wrapped | FileCheck --dump-input=always -v %binary_dir/tests/threads/threads.out
*/
#include "library.h"
#include <assert.h>
#include <ia2.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#define IA2_DEFINE_TEST_HANDLER
#include <test_fault_handler.h>
#include <unistd.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

int data_in_main = 30;

void defined_in_main(void) { printf("main data is %d\n", data_in_main); }

Fn fnptr_from_main = defined_in_main;

void *thread_fn(void *ptr);

void *thread_fn(void *ptr) {
  printf("tid %d ptr=%p\n", gettid(), ptr);

  printf("main-module thread pkru=%08x\n", ia2_get_pkru());

  library_showpkru();

  printf("ptr is %p, formatting int: %08zx\n", ptr, 4300 + (size_t)ptr);

  return NULL;
}
pthread_t thread1, thread2;

void create_threads(void) {
  int iret1, iret2;

#if !defined(PRE_REWRITER)
  iret1 = pthread_create(&thread1, NULL, thread_fn, (void *)1);
  iret2 = pthread_create(&thread2, NULL, thread_fn, (void *)2);
#endif
}

__thread int thread_local_var = 50;

void *access_ptr_thread_fn(void *ptr) {
  int *x = (int *)ptr;
  printf("c1t3 accessing c1t1 thread-local: %d\n", *x);
  printf("c2t3 accessing c1t1 thread-local: %d\n",
         CHECK_VIOLATION(library_access_int_ptr(x)));
  return NULL;
}

int main() {
  printf("main-module main pkru=%08x\n", ia2_get_pkru());
  library_showpkru();
  printf("main-module main pkru=%08x\n", ia2_get_pkru());

  pthread_t lib_thread = library_spawn_thread();

  create_threads();
  library_foo();
  defined_in_main();
  library_call_fn(fnptr_from_main);

  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);
  pthread_join(lib_thread, NULL);

  pthread_t fault_thread;
#if !defined(PRE_REWRITER)
  int thread_create_ret = pthread_create(
      &fault_thread, NULL, access_ptr_thread_fn, (void *)&thread_local_var);
#endif
  pthread_join(fault_thread, NULL);

  return 0;
}
