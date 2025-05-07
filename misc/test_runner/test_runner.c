#define _GNU_SOURCE

/* Define this macro to make the pointers in struct fake_criterion_test function pointers */
#define IA2_TEST_RUNNER_SOURCE
#include "include/ia2_test_runner.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

struct fake_criterion_test *fake_criterion_tests IA2_SHARED_DATA = NULL;

/* This is shared data to allow checking for violations from different compartments */
bool expect_fault IA2_SHARED_DATA = false;

/* Create a stack for the signal handler to use since the tests may trigger it from any compartment */
char sighandler_stack[4 * 1024] IA2_SHARED_DATA __attribute__((aligned(16))) = {0};

/* Pointer to the start of the signal handler stack */
char *sighandler_sp IA2_SHARED_DATA = &sighandler_stack[(4 * 1024) - 8];

// This function must be declared naked because it's not necessarily safe for it
// to write to the stack in its prelude (the stack isn't written to when the
// function itself is called because it's only invoked as a signal handler).
#if defined(__x86_64__)
__attribute__((naked)) void handle_segfault(int sig) {
  // This asm must preserve %rdi which contains the argument since
  // print_mpk_message reads it
  __asm__(
      // This signal handler is defined in the main binary, but it doesn't run with
      // the same pkru state as the interrupted context. This means we have to
      // remove all MPK restrictions to ensure can run it correctly.
      "xorl %ecx, %ecx\n"
      "xorl %edx, %edx\n"
      "xorl %eax, %eax\n"
      "wrpkru\n"
      // Switch the stack to a shared buffer. There's only one u32 argument and
      // no returns so we don't need a full callgate wrapper here.
      "movq sighandler_sp@GOTPCREL(%rip), %rsp\n"
      "movq (%rsp), %rsp\n"
      "callq print_mpk_message");
}
#elif defined(__aarch64__)
#warning "Review test_fault_handler implementation after enabling x18 switching"
void print_mpk_message(int sig);
void handle_segfault(int sig) {
  print_mpk_message(sig);
}
#endif

/*
 * The test output is used for manual sanity-checks to ensure check whether a segfault occurred and
 * if it was expected or not.
 */
void print_mpk_message(int sig) {
  if (sig == SIGSEGV) {
    /* segfault happened at the expected place so exit with status code zero */
    const char *ok_msg = "CHECK_VIOLATION: seg faulted as expected\n";
    /* segfault happened at an unexpected place so exit with non-zero status */
    const char *early_fault_msg = "CHECK_VIOLATION: unexpected seg fault\n";
    const char *msg;
    if (expect_fault) {
      msg = ok_msg;
    } else {
      msg = early_fault_msg;
    }
    /* Write directly to stdout since printf is not async-signal-safe */
    write(1, msg, strlen(msg));
    if (!expect_fault) {
      _exit(-1);
    }
  }
  _exit(0);
}

int main() {
  struct sigaction act = {
      .sa_handler = handle_segfault,
  };
  /*
   * Installs a signal handler that will be inherited by the child processes created for each
   * invocation of the Test macro
   */
  sigaction(SIGSEGV, &act, NULL);

  // Reverse tests, as the `__attribute__((constructor))` approach with a linked list makes them backwards.
  size_t i = 0;
  for (struct fake_criterion_test *test_info = fake_criterion_tests; test_info; test_info = test_info->next) {
    i++;
  }
  const size_t num_tests = i;
  struct fake_criterion_test tests[num_tests];
  for (struct fake_criterion_test *test_info = fake_criterion_tests; test_info; test_info = test_info->next) {
    i--;
    tests[i] = *test_info;
  }

  // Check test values and merge them (e.x. `signal` and `exit_code`).
  for (i = 0; i < num_tests; i++) {
    struct fake_criterion_test *test = &tests[i];
    if (test->signal != 0) {
      // If an expected signal was set, check if it was valid.
      assert(test->signal > 0 && test->signal < _NSIG);
      if (test->exit_code != 0) {
        fprintf(stderr, "can't expect both signal %d (SIG%s) and exit status %d\n",
                test->signal, sigabbrev_np(test->signal), test->exit_code);
        return EXIT_FAILURE;
      }
      // Sometimes the signal doesn't show up with `WIFSIGNALED`,
      // so we check the exit status as well.
      test->exit_code = 128 + test->signal;
    }
    if (test->exit_code > 128 && test->exit_code < 128 + _NSIG) {
      test->signal = test->exit_code - 128;
    }
  }

  // Run a single test without checking exit status if `IA2_TEST_NAME` is set.
  const char *const single_test_name = getenv("IA2_TEST_NAME");

  for (i = 0; i < num_tests; i++) {
    const struct fake_criterion_test *test = &tests[i];

    fprintf(stderr, "running suite '%s' test '%s', expecting ", test->suite, test->name);
    if (test->signal != 0) {
      fprintf(stderr, "signal %d (SIG%s)", test->signal, sigabbrev_np(test->signal));
    } else {
      fprintf(stderr, "exit status %d", test->exit_code);
      if (test->exit_code == EXIT_SUCCESS) {
        fprintf(stderr, " (%s)", STRINGIFY(EXIT_SUCCESS));
      } else if (test->exit_code == EXIT_FAILURE) {
        fprintf(stderr, " (%s)", STRINGIFY(EXIT_FAILURE));
      }
    }
    fprintf(stderr, "...\n");

    if (single_test_name) {
      if (strcmp(test->name, single_test_name) != 0) {
        continue;
      }
      fprintf(stderr, "running single test alone (exit status will not be checked):\n");
    }

    /* Do not fork or check exit status if only running one test. */
    pid_t pid = 0;
    if (!single_test_name) {
      pid = fork();
    }
    bool in_child = pid == 0;
    if (in_child) {
      /*
       * This .c is not rewritten so these indirect callsites have no callgates and their callees must
       * not be wrapped. That means the Test macro should not expose function pointer types to
       * rewritten source files (i.e. the test sources).
       */
      if (test->init) {
        (*test->init)();
      }
      (*test->test)();
      return EXIT_SUCCESS;
    }
    // otherwise, in parent
    int status;
    pid_t waited_pid = waitpid(pid, &status, 0);
    if (waited_pid < 0) {
      perror("waitpid");
      return 2;
    }

    assert(WIFEXITED(status) ^ WIFSIGNALED(status));
    if (WIFSIGNALED(status)) {
      fprintf(stderr, "killed by signal %d (SIG%s)",
              WTERMSIG(status), sigabbrev_np(WTERMSIG(status)));
      if (test->signal != 0 && test->signal == WTERMSIG(status)) {
        fprintf(stderr, ", as expected\n");
        continue;
      }
      if (test->signal != 0) {
        fprintf(stderr, ", but expected signal %d (SIG%s)\n",
                test->signal, sigabbrev_np(test->signal));
      } else {
        fprintf(stderr, ", but expected exit status %d\n", test->exit_code);
      }
      return EXIT_FAILURE;
    }
    if (WIFEXITED(status)) {
      fprintf(stderr, "exited with status %d", WEXITSTATUS(status));
      if (test->exit_code == WEXITSTATUS(status)) {
        fprintf(stderr, ", as expected\n");
        continue;
      }
      fprintf(stderr, ", but expected exit status %d", test->exit_code);
      if (test->signal != 0) {
        fprintf(stderr, " as signal %d (SIG%s)", test->signal, sigabbrev_np(test->signal));
      }
      fprintf(stderr, "\n");
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
