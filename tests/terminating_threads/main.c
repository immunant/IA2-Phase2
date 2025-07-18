#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

#include <pthread.h>
#include <signal.h>

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
  exit(0); // TODO Skip for now, as `pthread_cancel` `SIGSEGV`s (#606).

  const int result = pthread_cancel(thread) != 0;
  if (result != 0) {
    return result;
  }
  // Cancellation is asynchronous, so we still need to
  // join it to ensure cancellation completes.
  return pthread_join(thread, NULL);
}

struct start_wrapper_args {
  start_fn start;
  pthread_barrier_t *barrier;
};

static void *start_wrapper(void *arg) {
  const struct start_wrapper_args *const args = (const struct start_wrapper_args *)arg;

  // Save the start fn before the barrier, as `args` might be deallocated after the barrier.
  const start_fn start = args->start;

  pthread_barrier_wait(args->barrier);
  return start(NULL);
}

void run_test(size_t num_threads, start_fn start, end_fn end, start_fn main) {
  if (num_threads > 0) {
    pthread_t threads[num_threads];
    struct start_wrapper_args args[num_threads];
    pthread_barrier_t barrier;
    cr_assert(pthread_barrier_init(&barrier, NULL, (unsigned)num_threads + 1) == 0);

    for (size_t i = 0; i < num_threads; i++) {
      args[i].start = start;
      args[i].barrier = &barrier;
#if IA2_ENABLE
      pthread_create(&threads[i], NULL, start_wrapper, (void *)&args[i]);
#endif
    }

    // Make sure all threads read `args` before it goes out of scope here.
    pthread_barrier_wait(&barrier);

    for (size_t i = 0; i < num_threads; i++) {
      // Don't call fn ptr inside a macro, as the rewriter won't rewrite it.
      const int result = end(threads[i]);
      cr_assert(result == 0);
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
