#include "include/ia2_test_runner.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern struct fake_criterion_test __start_fake_criterion_tests;
extern struct fake_criterion_test __stop_fake_criterion_tests;

int main() {
  struct fake_criterion_test *test_info = &__start_fake_criterion_tests;
  for (; test_info < &__stop_fake_criterion_tests; test_info++) {
    if (!test_info->test) {
      break;
    }
    pid_t pid = fork();
    bool in_child = pid == 0;
    if (in_child) {
      if (test_info->init) {
        (*test_info->init)();
      }
      (*test_info->test)();
      return 0;
    }
    // otherwise, in parent
    int stat;
    pid_t waited_pid = waitpid(pid, &stat, 0);
    if (waited_pid < 0) {
      perror("waitpid");
      return 2;
    }
    if WIFSIGNALED(stat) {
      fprintf(stderr, "forked test child was terminated by signal %d\n", WTERMSIG(stat));
      return 1;
    }
    int exit_status = WEXITSTATUS(stat);
    if (exit_status != test_info->exit_code) {
      fprintf(stderr, "forked test child exited with status %d, but %d was expected\n", exit_status, test_info->exit_code);
      return 1;
    }
  }
  return 0;
}

// This is shared data to allow checking for violations in multiple
// compartments. We avoid using IA2_SHARED_DATA here to avoid including ia2.h
// since that would pull in libia2 as a dependency (the libia2 build generates a
// header included in ia2.h).
bool expect_fault __attribute__((section("ia2_shared_data"))) = false;

// Create a stack for the signal handler to use
char sighandler_stack[4 * 1024] __attribute__((section("ia2_shared_data")))
__attribute__((aligned(16))) = {0};
char *sighandler_sp __attribute__((section("ia2_shared_data"))) =
    &sighandler_stack[(4 * 1024) - 8];

// This function must be declared naked because it's not necessarily safe for it
// to write to the stack in its prelude (the stack isn't written to when the
// function itself is called because it's only invoked as a signal handler).
#if defined(__x86_64__)
__attribute__((naked)) void handle_segfault(int sig) {
  // This asm must preserve %rdi which contains the argument since
  // print_mpk_message reads it
  __asm__(
      // Signal handlers are defined in the main binary, but they don't run with
      // the same pkru state as the interrupted context. This means we have to
      // remove all MPK restrictions to ensure can run it correctly.
      "xorl %ecx, %ecx\n"
      "xorl %edx, %edx\n"
      "xorl %eax, %eax\n"
      "wrpkru\n"
      // Switch the stack to a shared buffer. There's only one u32 argument and
      // no returns so we don't need a full wrapper here.
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

// The test output should be checked to see that the segfault occurred at the
// expected place.
void print_mpk_message(int sig) {
  if (sig == SIGSEGV) {
    // Write directly to stdout since printf is not async-signal-safe
    const char *ok_msg = "CHECK_VIOLATION: seg faulted as expected\n";
    const char *early_fault_msg = "CHECK_VIOLATION: unexpected seg fault\n";
    const char *msg;
    if (expect_fault) {
      msg = ok_msg;
    } else {
      msg = early_fault_msg;
    }
    write(1, msg, strlen(msg));
    if (!expect_fault) {
      _exit(-1);
    }
  }
  _exit(0);
}

// Installs the previously defined signal handler and disables buffering on
// stdout to allow using printf prior to the sighandler
__attribute__((constructor)) void install_segfault_handler(void) {
  setbuf(stdout, NULL);
  signal(SIGSEGV, handle_segfault);
}
