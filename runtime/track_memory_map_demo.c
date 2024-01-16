#define _GNU_SOURCE
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include "track_memory_map.h"

void usage(const char *name) { fprintf(stderr, "usage: %s COMMAND [ARGS...]\n", name); }

pid_t spawn_with_tracker(char *const *argv) {
  pid_t child_pid = fork();
  bool in_child = (child_pid == 0);
  if (in_child) {
    int ptrace_ret = ptrace(PTRACE_TRACEME, 0, 0, 0);
    if (ptrace_ret < 0) {
      perror("PTRACE_TRACEME");
      exit(1);
    }

    /* wait for tracer */
    kill(getpid(), SIGSTOP);

    /* run tracked program */
    execvp(argv[0], argv);
    perror("exec");
    exit(1);
  }

  if (child_pid < 0) {
    perror("fork");
    return -1;
  }

  /* wait to get hold of the tracee */
  int status;
  pid_t ret_pid;
  while ((ret_pid = waitpid(child_pid, &status, 0)) == 0) {
    if (ret_pid < 0) {
      perror("waitpid");
      return -1;
    }
  }

  /* do not let the tracee continue if our process dies */
  ptrace(PTRACE_SETOPTIONS, child_pid, 0, PTRACE_O_EXITKILL|PTRACE_O_TRACESYSGOOD);

  /* run the child up to the next syscall */
  ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);
  waitpid(child_pid, NULL, 0);

  return child_pid;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(basename(argv[0]));
    return 1;
  }

  pid_t pid = spawn_with_tracker((char *const *)&argv[1]);
  if (pid < 0) {
    fprintf(stderr, "could not spawn child process\n");
    exit(1);
  }

  int exit_status = 0;
  bool success = track_memory_map(pid, &exit_status, TRACE_MODE_PTRACE_SYSCALL);
  /* ensure the child is dead */
  kill(pid, SIGKILL);

  if (success) {
    fprintf(stderr, "inferior exited with code %d\n", exit_status);
  } else {
    fprintf(stderr, "error tracking memory map\n");
  }
  return !success;
}
