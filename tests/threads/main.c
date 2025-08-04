/*
RUN: sh -c 'if [ ! -s "threads_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/
#include <ia2_test_runner.h>

#include "library.h"
#include <assert.h>
#include <ia2.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>

#include <unistd.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
}

int data_in_main = 30;

void defined_in_main(void) {
  cr_log_info("main data is %d\n", data_in_main);
  cr_assert_eq(data_in_main, 30);
}

Fn fnptr_from_main = defined_in_main;

void *thread_fn(void *ptr);

void *thread_fn(void *ptr) {
  cr_log_info("tid %d ptr=%p\n", gettid(), ptr);

  cr_log_info("main-module thread pkru=%08zx\n", ia2_get_tag());
#if defined(__aarch64__)
  cr_assert_eq(ia2_get_tag(), 1);
#else
  cr_assert_eq(ia2_get_tag(), 0xfffffff0);
#endif

  library_showpkru();

  cr_log_info("ptr is %p, formatting int: %08zx\n", ptr, 4300 + (size_t)ptr);

  return NULL;
}
pthread_t thread1, thread2;

void create_threads(void) {
  int iret1, iret2;

#if IA2_ENABLE
  iret1 = pthread_create(&thread1, NULL, thread_fn, (void *)1);
  iret2 = pthread_create(&thread2, NULL, thread_fn, (void *)2);
#endif
}

__thread int thread_local_var = 50;

void *access_ptr_thread_fn(void *ptr) {
  cr_log_info("accessing pointer %p from access_ptr_thread_fn\n", ptr);
  int *x = (int *)ptr;
  cr_log_info("c1t3 accessing c1t1 thread-local: %d\n", *x);
  cr_log_info("c2t3 accessing c1t1 thread-local: %d\n",
              CHECK_VIOLATION(library_access_int_ptr(x)));
  return NULL;
}

Test(threads, main) {
  cr_log_info("main-module main pkru=%08zx\n", ia2_get_tag());
#if defined(__aarch64__)
  cr_assert_eq(ia2_get_tag(), 1);
#else
  cr_assert_eq(ia2_get_tag(), 0xfffffff0);
#endif
  library_showpkru();
  cr_log_info("main-module main pkru=%08zx\n", ia2_get_tag());

  pthread_t lib_thread = library_spawn_thread();

  create_threads();
  library_foo();
  defined_in_main();
  library_call_fn(fnptr_from_main);

  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);
  pthread_join(lib_thread, NULL);

  pthread_t fault_thread;
#if IA2_ENABLE
  cr_log_info("passing pointer %p to access_ptr_thread_fn\n", (void *)&thread_local_var);
  int thread_create_ret = pthread_create(
      &fault_thread, NULL, access_ptr_thread_fn, (void *)&thread_local_var);
#endif
  pthread_join(fault_thread, NULL);
}

// Ensure we are not corrupting thread argument or return value.
#define TEST_THREAD_DATA_VALUE 0xabcdef0123456789
void *arg_ret_thread_fn(void *ptr) {
  cr_assert_eq(ptr, (void *)TEST_THREAD_DATA_VALUE);
  return (void *)((uintptr_t)ptr / 2);
}

#if IA2_ENABLE
Test(threads, arg_ret) {
  pthread_t thread;
  int create_ret = pthread_create(&thread, NULL, arg_ret_thread_fn, (void *)TEST_THREAD_DATA_VALUE);
  cr_assert_eq(create_ret, 0);
  void *out = 0;
  int join_ret = pthread_join(thread, &out);
  cr_assert_eq(join_ret, 0);
  cr_assert_eq((uintptr_t)out, TEST_THREAD_DATA_VALUE / 2);
}
#endif
