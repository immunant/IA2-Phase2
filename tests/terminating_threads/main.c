#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

#include <pthread.h>
#include <signal.h>

#if IA2_ENABLE

typedef void *(*start_fn)(void *arg);
typedef int (*end_fn)(pthread_t thread);

void *start_return(void *_arg) {
  return NULL;
}

void *start_exit(void *_arg) {
  exit(0);
}

void *start_abort(void *_arg) {
  abort();
  return NULL;
}

void *start_pthread_exit(void *_arg) {
  exit(0); // TODO Skip for now, as `pthread_exit` `SIGILL`s (#605).

  pthread_exit(NULL);
  return NULL;
}

void *start_pause(void *_arg) {
  pause();
  return NULL;
}

void *start_sleep_100_us(void *_arg) {
  usleep(100);
  return NULL;
}

int end_none(pthread_t _thread) {
  return 0;
}

int end_join(pthread_t thread) {
  return pthread_join(thread, NULL);
}

int end_cancel(pthread_t thread) {
  // return pthread_cancel(thread); // TODO `SIGSEGV`s.
  exit(0);
}

void run_test(size_t num_threads, start_fn start, end_fn end, start_fn main) {
  if (num_threads > 0) {
    pthread_t threads[num_threads];
    for (size_t i = 0; i < num_threads; i++) {
      pthread_create(&threads[i], NULL, start, NULL);
    }
    for (size_t i = 0; i < num_threads; i++) {
      cr_assert(end(threads[i]) == 0);
    }
  }
  main(NULL);
}

// 1 thread

Test(terminating_threads, threads_1_return) {
  run_test(0, start_pause, end_none, start_return);
}

Test(terminating_threads, threads_1_exit) {
  run_test(0, start_pause, end_none, start_exit);
}

Test(terminating_threads, threads_1_abort, .signal = SIGABRT) {
  run_test(0, start_pause, end_none, start_abort);
}

Test(terminating_threads, threads_1_pthread_exit) {
  run_test(0, start_pause, end_none, start_pthread_exit);
}

// 2 threads, main thread

Test(terminating_threads, threads_2_main_thread_return) {
  run_test(1, start_pause, end_none, start_return);
}

Test(terminating_threads, threads_2_main_thread_exit) {
  run_test(1, start_pause, end_none, start_exit);
}

Test(terminating_threads, threads_2_main_thread_abort, .signal = SIGABRT) {
  run_test(1, start_pause, end_none, start_abort);
}

Test(terminating_threads, threads_2_main_thread_pthread_exit) {
  run_test(1, start_pause, end_none, start_pthread_exit);
}

// 11 threads, main thread

Test(terminating_threads, threads_11_main_thread_return) {
  run_test(10, start_pause, end_none, start_return);
}

Test(terminating_threads, threads_11_main_thread_exit) {
  run_test(10, start_pause, end_none, start_exit);
}

Test(terminating_threads, threads_11_main_thread_abort, .signal = SIGABRT) {
  run_test(10, start_pause, end_none, start_abort);
}

Test(terminating_threads, threads_11_main_thread_pthread_exit) {
  run_test(10, start_pause, end_none, start_pthread_exit);
}

// 2 threads, other thread

Test(terminating_threads, threads_2_other_thread_return) {
  run_test(1, start_return, end_join, start_return);
}

Test(terminating_threads, threads_2_other_thread_exit) {
  run_test(1, start_exit, end_join, start_return);
}

Test(terminating_threads, threads_2_other_thread_abort, .signal = SIGABRT) {
  run_test(1, start_abort, end_join, start_return);
}

Test(terminating_threads, threads_2_other_thread_pthread_exit) {
  run_test(1, start_pthread_exit, end_join, start_return);
}

Test(terminating_threads, threads_2_other_thread_pthread_join) {
  run_test(1, start_sleep_100_us, end_join, start_return);
}

Test(terminating_threads, threads_2_other_thread_pthread_cancel) {
  run_test(1, start_pause, end_cancel, start_return);
}

// 11 threads, other threads

Test(terminating_threads, threads_11_other_threads_return) {
  run_test(10, start_return, end_join, start_return);
}

Test(terminating_threads, threads_11_other_threads_exit) {
  run_test(10, start_exit, end_join, start_return);
}

Test(terminating_threads, threads_11_other_threads_abort, .signal = SIGABRT) {
  run_test(10, start_abort, end_join, start_return);
}

Test(terminating_threads, threads_11_other_threads_pthread_exit) {
  run_test(10, start_pthread_exit, end_join, start_return);
}

Test(terminating_threads, threads_11_other_threads_pthread_join) {
  run_test(10, start_sleep_100_us, end_join, start_return);
}

Test(terminating_threads, threads_11_other_threads_pthread_cancel) {
  run_test(10, start_pause, end_cancel, start_return);
}

#endif
