#define _GNU_SOURCE
#include <sys/wait.h>

#include "memory_map.h"

enum trace_mode {
  /* run the program with PTRACE_CONT and only expect interruptions at traced
  syscalls or signal receipt */
  TRACE_MODE_SECCOMP,
  /* run the program with PTRACE_SYSCALL to step to the next syscall edge */
  TRACE_MODE_PTRACE_SYSCALL,
};

bool track_memory_map(pid_t pid, struct memory_map *map, int *exit_status_out, enum trace_mode mode);
