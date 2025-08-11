#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

#include <pthread.h>
#include <signal.h>
#include <unistd.h>

/// Check if `ptr` is currently a mapped memory address using `mincore`.
static bool addr_is_mapped(void *const ptr) {
  const uintptr_t page_mask = ~(PAGE_SIZE - 1);
  void *const aligned_ptr = (void *)((uintptr_t)ptr & page_mask);

  unsigned char vec[1] = {0}; // We're only checking 1 page.
  const int result = mincore(aligned_ptr, PAGE_SIZE, vec);
  if (result == -1) {
    if (errno == ENOMEM) {
      // `ENOMEM` for `mincore` means that the page `aligned_ptr..aligned_ptr + PAGE_SIZE`,
      // which itself contains `ptr`, contains unmapped memory pages.
      cr_log_info("%p is unmapped", ptr);
      return false;
    }
    cr_fatal("mincore failed: %s", strerrorname_np(errno));
  }
  cr_log_info("%p is mapped", ptr);
  return true;
}

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
  ia2_register_compartment("libterminating_threads_lib.so", 2, NULL);
}

typedef void *(*start_fn)(void *arg);
typedef int (*end_fn)(pthread_t thread);

static void *start_return(void *_arg) {
  return NULL;
}

// Takes either an int * to the thread index or NULL if called from the main thread.
static void *start_exit(void *arg_index) {
  // `exit` is not thread-safe so only call it from the main thread or the first thread created
  // (index 0)
  int *index = (int *)arg_index;
  if (index && *index != 0) {
    return NULL;
  }
  exit(0);
}

static void *start_abort(void *_arg) {
  abort();
  return NULL;
}

// pthread_exit triggers a signal handler defined by libc/may cause libc to call exit (which does
// not go through our wrapper) so tests using this function are currently disabled (see issue #605).
#if 0
static void *start_pthread_exit(void *_arg) {
  pthread_exit(NULL);
  return NULL;
}
#endif

static void *start_pause(void *_arg) {
  pause();
  return NULL;
}

static void *start_sleep_100_us(void *_arg) {
  usleep(100);
  return NULL;
}

static int end_none(pthread_t _thread) {
  return 0;
}

static int end_join(pthread_t thread) {
  return pthread_join(thread, NULL);
}

static int end_cancel(pthread_t thread) {
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
  void *stack_ptr;
  size_t index;
};

/// Run some pre-thread start code in the new thread.
///
/// `arg` should be a `struct start_wrapper_args`.
///
/// The wrapper:
/// * saves `args->start` in case it's deallocated in the parent thread.
/// * stores a stack ptr from the new thread.
/// * names the thread to help with debugging.
/// * waits on `args->barrier` so that all thread wrappers finish before the parent thread finishes.
static void *start_wrapper(void *arg) {
  struct start_wrapper_args *const args = (struct start_wrapper_args *)arg;

  // Save the start fn before the barrier, as `args` might be deallocated after the barrier.
  const start_fn start = args->start;

  int stack_arg;
  args->stack_ptr = (void *)&stack_arg;

  const char *start_name = "?";
  if (start == start_return) {
    start_name = "return";
  } else if (start == start_exit) {
    start_name = "exit";
  } else if (start == start_abort) {
    start_name = "abort";
#if 0
  } else if (start == start_pthread_exit) {
    start_name = "pthread_exit";
#endif
  } else if (start == start_pause) {
    start_name = "pause";
  } else if (start == start_sleep_100_us) {
    start_name = "sleep_100_us";
  }

  char thread_name[16] = {0};
  snprintf(thread_name, sizeof(thread_name), "%zu, %s", args->index, start_name);
  const int result = pthread_setname_np(pthread_self(), thread_name);
  if (result != 0) {
    cr_fatal("pthread_setname_np failed: %s", strerrorname_np(result));
  }
  cr_log_info("thread %ld is named %s", (long)gettid(), thread_name);

  pthread_barrier_wait(args->barrier);
  return start((void*)&args->index);
}

static void run_test(size_t num_threads, start_fn start, end_fn end, start_fn main) {
  if (num_threads > 0) {
    pthread_t threads[num_threads];
    struct start_wrapper_args args[num_threads];
    pthread_barrier_t barrier;
    cr_assert(pthread_barrier_init(&barrier, NULL, (unsigned)num_threads + 1) == 0);

    for (size_t i = 0; i < num_threads; i++) {
      args[i] = (struct start_wrapper_args){
          .start = start,
          .barrier = &barrier,
          .stack_ptr = NULL,
          .index = i,
      };
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

      // NOTE: This only checks that compartment 1 (`main.c`)'s stacks are freed.
      const bool stack_is_mapped = addr_is_mapped(args[i].stack_ptr);
      if (start == start_pause && end == end_none) {
        // Threads are still alive, so their stack ptrs should still be mapped.
        cr_assert(stack_is_mapped);
      } else if (end == end_join || false /* end == end_cancel */) {
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

#if 0
Test(terminating_threads, threads_1_pthread_exit) {
  run_test(0, start_pause, end_none, start_pthread_exit);
}
#endif

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

#if 0
Test(terminating_threads, threads_2_main_thread_pthread_exit) {
  run_test(1, start_pause, end_none, start_pthread_exit);
}
#endif

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

#if 0
Test(terminating_threads, threads_11_main_thread_pthread_exit) {
  run_test(10, start_pause, end_none, start_pthread_exit);
}
#endif

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

#if 0
Test(terminating_threads, threads_2_other_thread_pthread_exit) {
  run_test(1, start_pthread_exit, end_join, start_return);
}
#endif

Test(terminating_threads, threads_2_other_thread_pthread_join) {
  run_test(1, start_sleep_100_us, end_join, start_return);
}

Test(terminating_threads, threads_2_other_thread_pthread_cancel) {
  run_test(1, start_pause, end_cancel, start_return);
}

// 11 threads, other threads

#ifndef __aarch64__

Test(terminating_threads, threads_11_other_threads_return) {
  run_test(10, start_return, end_join, start_return);
}

#endif

Test(terminating_threads, threads_11_other_threads_exit) {
  run_test(10, start_exit, end_join, start_return);
}

Test(terminating_threads, threads_11_other_threads_abort, .signal = SIGABRT) {
  run_test(10, start_abort, end_join, start_return);
}

#if 0
Test(terminating_threads, threads_11_other_threads_pthread_exit) {
  run_test(10, start_pthread_exit, end_join, start_return);
}
#endif

Test(terminating_threads, threads_11_other_threads_pthread_join) {
  run_test(10, start_sleep_100_us, end_join, start_return);
}

Test(terminating_threads, threads_11_other_threads_pthread_cancel) {
  run_test(10, start_pause, end_cancel, start_return);
}
