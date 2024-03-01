#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include "get_inferior_pkru.h"

void usage(const char *name) { fprintf(stderr, "usage: %s <pid>\n", name); }

int main(int argc, char **argv) {
  if (argc != 2) {
    usage(basename(argv[0]));
    return 1;
  }

  pid_t pid = atoi(argv[1]);
  if (pid == 0) {
    usage(basename(argv[0]));
    return 1;
  }

  if (ptrace(PTRACE_ATTACH, pid, 0, 0) < 0) {
    perror("could not ptrace(PTRACE_ATTACH)");
    return 1;
  } else {
    /* wait to get hold of the tracee */
    pid_t ret = waitpid(pid, NULL, WUNTRACED);
    if (ret < 0) {
      perror("waitpid");
      return 1;
    }
  }

  uint32_t pkru = 0;
  bool res = get_inferior_pkru(pid, &pkru);
  if (res) {
    printf("pkru=%08x\n", pkru);
  }

  return res;
}
