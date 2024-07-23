#include "include/criterion/criterion.h"
#include <stdbool.h>
#include <stdio.h>
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
    int exit_status = WEXITSTATUS(stat);
    if (exit_status != test_info->exit_code) {
      fprintf(stderr, "forked test child exited with status %d, but %d was expected\n", exit_status, test_info->exit_code);
      return 1;
    }
  }
  return 0;
}
