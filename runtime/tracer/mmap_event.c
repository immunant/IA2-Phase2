#include <asm/unistd_64.h>

#include "mmap_event.h"

enum mmap_event event_from_syscall(uint64_t rax) {
  switch (rax) {
  case __NR_mmap:
    return EVENT_MMAP;
  case __NR_munmap:
    return EVENT_MUNMAP;
  case __NR_mremap:
    return EVENT_MREMAP;
  case __NR_madvise:
    return EVENT_MADVISE;
  case __NR_mprotect:
    return EVENT_MPROTECT;
  case __NR_pkey_mprotect:
    return EVENT_PKEY_MPROTECT;
  case __NR_wait4:
  case __NR_clone:
  case __NR_clone3:
    return EVENT_CLONE;
  case __NR_execve:
  case __NR_execveat:
    return EVENT_EXEC;
  default:
    return EVENT_NONE;
  }
}
