#include "landlock.h"
#include "seccomp_filter.h"
#include "track_memory_map.h"

#include <libgen.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

void usage(const char *name) { fprintf(stderr, "usage: %s COMMAND [ARGS...]\n", name); }

static void forbid_proc_self_mem_or_exit(void) {
  struct landlock_ctx ctx = {0};
  if (landlock_setup(&ctx) != 0) {
    exit(1);
  }

  const char *paths[] = {"/proc/self/mem", NULL};

  if (landlock_apply_path_blacklist(&ctx, paths) != 0) {
    exit(1);
  }
}

static pid_t fork_and_trace(char *const *argv) {
  pid_t child_pid = fork();
  bool in_child = (child_pid == 0);
  if (in_child) {
    /* forbid access to /proc/self/mem via landlock LSM */
    forbid_proc_self_mem_or_exit();

    /* ptrace(PTRACE_TRACEME) before we seccomp() away the ability to ptrace */
    int ptrace_ret = ptrace(PTRACE_TRACEME, 0, 0, 0);
    if (ptrace_ret < 0) {
      perror("PTRACE_TRACEME");
      exit(1);
    }

    /* in order to use seccomp() without CAP_SYS_SECCOMP, we must opt out of being
    able to gain privs via exec() of setuid binaries as they would inherit our
    seccomp filters. handily, we did that when we forbade /proc/self/mem with
    landlock, so we can just go ahead and call seccomp() here */
    int fd = configure_seccomp(&ia2_filter_prog);
    if (fd < 0) {
      fprintf(stderr, "could not set up seccomp\n");
      exit(1);
    }

    /* wait for tracer */
    kill(getpid(), SIGSTOP);

    /* run sandboxed program */
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

  unsigned long options = 0;
  /* do not let the tracee continue if our process dies */
  options |= PTRACE_O_EXITKILL;
  /* stop tracee when seccomp returns RET_TRACE */
  options |= PTRACE_O_TRACESECCOMP;
  /* we want to know about clone() and fork() */
  options |= PTRACE_O_TRACECLONE | PTRACE_O_TRACEVFORK | PTRACE_O_TRACEFORK;
  /* and exec() */
  options |= PTRACE_O_TRACEEXEC;
  /* distinguish syscall stops from SIGTRAP receipt */
  options |= PTRACE_O_TRACESYSGOOD;

  ptrace(PTRACE_SETOPTIONS, child_pid, 0, options);

  /* run the child up to the first traced syscall */
  if (ptrace(PTRACE_CONT, child_pid, NULL, NULL) < 0) {
    perror("PTRACE_CONT");
    return -1;
  }
  if (waitpid(child_pid, NULL, 0) < 0) {
    perror("waitpid");
    return -1;
  }

  if (ptrace(PTRACE_CONT, child_pid, NULL, NULL) < 0) {
    perror("PTRACE_CONT");
    return -1;
  }

  return child_pid;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(basename(argv[0]));
    return 1;
  }

  pid_t pid = fork_and_trace((char *const *)&argv[1]);
  if (pid < 0) {
    fprintf(stderr, "could not spawn child process\n");
    exit(1);
  }

  int exit_status = 0;
  bool success = track_memory_map(pid, &exit_status, TRACE_MODE_SECCOMP);
  /* ensure the child is dead */
  kill(pid, SIGKILL);

  if (success)
    exit(exit_status);
  else {
    fprintf(stderr, "error tracing sandboxed child!\n");
    return 1;
  }
}