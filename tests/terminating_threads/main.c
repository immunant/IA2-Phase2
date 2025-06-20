#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

#include <pthread.h>
#include <signal.h>
#include <unistd.h>

bool addr_is_mapped(void *const ptr) {
  printf("addr_is_mapped(%p)\n", ptr);
  const long page_size = sysconf(_SC_PAGESIZE);
  const bool page_size_is_power_of_two = (page_size & (page_size - 1)) == 0;
  cr_assert(page_size > 0 && page_size_is_power_of_two);

  const uintptr_t page_mask = ~(page_size - 1);
  void *const aligned_ptr = (void *)((uintptr_t)ptr & page_mask);
  printf("addr_is_mapped(%p)\n", aligned_ptr);

  unsigned char vec = 0;
  const int result = mincore(aligned_ptr, page_size, &vec);
  switch (mincore(aligned_ptr, page_size, &vec)) {
  case 0:
    printf("addr_is_mapped: true\n");
    return true;
  case -1:
    if (errno == ENOMEM) {
      printf("addr_is_mapped: false\n");
      return false;
    }
  default:
    cr_assert(false);
  }
}

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
  exit(0); // TODO Skip for now, as `pthread_exit` `SIGILL`s.

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
  exit(0); // TODO Skip for now, as `pthread_cancel` `SIGSEGV`s.

  const int result = pthread_cancel(thread) != 0;
  if (result != 0) {
    return result;
  }
  // Cancellation is asynchronous, so we still need to
  // join it to ensure cancellation completes.
  return pthread_join(thread, NULL);
}

struct start_wrapper_record_stack_args {
  size_t index;
  start_fn start;
  pthread_barrier_t *barrier;
  void *stack_ptr;
};

void *start_wrapper_record_stack(void *const arg) {
  struct start_wrapper_record_stack_args *const args = arg;

  const char *start_name = "?";
  if (args->start == start_return) {
    start_name = "return";
  } else if (args->start == start_exit) {
    start_name = "exit";
  } else if (args->start == start_abort) {
    start_name = "abort";
  } else if (args->start == start_pthread_exit) {
    start_name = "pthread_exit";
  } else if (args->start == start_pause) {
    start_name = "pause";
  } else if (args->start == start_sleep_100_us) {
    start_name = "sleep_100_us";
  }
  char thread_name[16] = {0};
  snprintf(thread_name, sizeof(thread_name), "%zu, %s", args->index, start_name);
  pthread_setname_np(pthread_self(), thread_name);
  printf("thread %ld is named %s\n", (long)gettid(), thread_name);

  int stack_arg;
  args->stack_ptr = (void *)&stack_arg;

  pthread_barrier_wait(args->barrier);

  return args->start(NULL);
}

void run_test(size_t num_threads, start_fn start, end_fn end, start_fn main) {
  if (num_threads > 0) {
    pthread_t threads[num_threads];
    struct start_wrapper_record_stack_args args[num_threads];
    pthread_barrier_t barrier;
    cr_assert(pthread_barrier_init(&barrier, NULL, (unsigned)num_threads + 1) == 0);
    for (size_t i = 0; i < num_threads; i++) {
      args[i].index = i;
      args[i].start = start;
      args[i].barrier = &barrier;
      args[i].stack_ptr = NULL;
      pthread_create(&threads[i], NULL, start_wrapper_record_stack, (void *)&args[i]);
    }
    pthread_barrier_wait(&barrier);
    for (size_t i = 0; i < num_threads; i++) {
      cr_assert(end(threads[i]) == 0);
      const bool stack_is_mapped = addr_is_mapped(args[i].stack_ptr);
      if (start == start_pause && end == end_none) {
        // Threads are still alive, so their stack ptrs should still be mapped.
        cr_assert(stack_is_mapped);
      } else if (end == end_join || end == end_cancel) {
        cr_assert(!stack_is_mapped);
      }
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
