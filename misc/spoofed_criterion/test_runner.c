#include <stdbool.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

extern int (*__start_fake_criterion_tests)(void);
extern int (*__stop_fake_criterion_tests)(void);

int main() {
  int (**test)(void) = &__start_fake_criterion_tests;
  for (; test < &__stop_fake_criterion_tests; test++) {
    if (!test) {
      break;
    }
    pid_t pid = fork();
    bool in_child = pid == 0;
    if (in_child) {
      (*test)(); // TODO: test return values are ignored, so tests should return void
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
    if (exit_status == 0) {
      return 0;
    } else {
      fprintf(stderr, "forked test child exited with status %d\n", exit_status);
      return exit_status;
    }
  }
}
