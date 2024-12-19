#define _GNU_SOURCE
#include <assert.h>
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

  unsigned long options = 0;
  /* do not let the tracee continue if our process dies */
  options |= PTRACE_O_EXITKILL;
  /* we want to know about clone() and fork() */
  options |= PTRACE_O_TRACECLONE | PTRACE_O_TRACEVFORK | PTRACE_O_TRACEFORK;
  /* and exec() */
  options |= PTRACE_O_TRACEEXEC;
  /* distinguish syscall stops from userspace SIGTRAP receipt */
  options |= PTRACE_O_TRACESYSGOOD;

  ptrace(PTRACE_SEIZE, child_pid, 0, options);

  /* wait to get hold of the tracee */
  int status;
  pid_t ret_pid;
  while ((ret_pid = waitpid(child_pid, &status, 0)) == 0) {
    if (ret_pid < 0) {
      perror("waitpid");
      return -1;
    }
  }

  /* run the child up to the next syscall */
  if (ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL) < 0) {
    perror("PTRACE_SYSCALL");
    return -1;
  }

  ret_pid = waitpid(child_pid, &status, 0);
  if (ret_pid < 0) {
    perror("waitpid");
    return -1;
  }
  if (ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL) < 0) {
    perror("PTRACE_SYSCALL");
    return -1;
  }

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

  int wait_status = 0;
  bool success = track_memory_map(pid, &wait_status, TRACE_MODE_PTRACE_SYSCALL);
  /* ensure the child is dead */
  kill(pid, SIGKILL);

  if (success) {
    if (WIFEXITED(wait_status)) {
      fprintf(stderr, "inferior exited with code %d\n", WEXITSTATUS(wait_status));
    }
    if (WIFSIGNALED(wait_status)) {
      fprintf(stderr, "inferior killed by signal %d\n", WTERMSIG(wait_status));
    }
    if (WIFSTOPPED(wait_status)) {
      fprintf(stderr, "inferior stopped by signal %d\n", WSTOPSIG(wait_status));
    }
    assert(false);
  } else {
    fprintf(stderr, "error tracking memory map\n");
  }
  return !success;
}
