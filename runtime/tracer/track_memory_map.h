#pragma once

#define _GNU_SOURCE
#include <stdbool.h>
#include <sys/wait.h>

enum trace_mode {
  /* run the program with PTRACE_CONT and only expect interruptions at traced
  syscalls or signal receipt */
  TRACE_MODE_SECCOMP,
  /* run the program with PTRACE_SYSCALL to step to the next syscall edge */
  TRACE_MODE_PTRACE_SYSCALL,
};

bool track_memory_map(pid_t pid, int *wait_stawtus_out, enum trace_mode mode);
