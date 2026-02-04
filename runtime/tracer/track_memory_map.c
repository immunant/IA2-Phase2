#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <elf.h>
#include <sys/uio.h>

#ifdef __x86_64__
#include "get_inferior_pkru.h"
#endif
#include "memory_map.h"
#include "mmap_event.h"
#include "track_memory_map.h"

#ifdef DEBUG
#define debug(...) fprintf(stderr, __VA_ARGS__)
#define debug_op(...) fprintf(stderr, __VA_ARGS__)
#define debug_policy(...) fprintf(stderr, __VA_ARGS__)
#define debug_event(...) fprintf(stderr, __VA_ARGS__)
#define debug_event_update(...) fprintf(stderr, __VA_ARGS__)
#define debug_exit(...) fprintf(stderr, __VA_ARGS__)
#define debug_proc(...) fprintf(stderr, __VA_ARGS__)
#define debug_wait(...) fprintf(stderr, __VA_ARGS__)
#define debug_forbid(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug(...)
#define debug_op(...)
#define debug_policy(...)
#define debug_event(...)
#define debug_event_update(...)
#define debug_exit(...)
#define debug_proc(...)
#define debug_wait(...)
#define debug_forbid(...)
#endif

static bool is_op_permitted(struct memory_map *map, int event,
                            union event_info *info) {
  switch (event) {
  case EVENT_MMAP:
    // non-FIXED maps of NULL don't need to check for overlap
    if (info->mmap.range.start == 0 && !(info->mmap.flags & MAP_FIXED)) {
      return true;
    }
    if (memory_map_all_overlapping_regions_have_pkey(map, info->mmap.range,
                                                     info->mmap.pkey))
      return true;

    break;
  case EVENT_MUNMAP:
    if (memory_map_all_overlapping_regions_have_pkey(map, info->munmap.range,
                                                     info->munmap.pkey))
      return true;
    break;
  case EVENT_MREMAP:
    if (memory_map_all_overlapping_regions_have_pkey(
            map, info->mremap.old_range, info->mremap.pkey))
      return true;
    break;
  case EVENT_MADVISE:
    if (memory_map_all_overlapping_regions_have_pkey(
            map, info->madvise.range, info->madvise.pkey))
      return true;
    break;
  case EVENT_MPROTECT: {
    /* allow mprotecting memory that has not been mprotected */
    bool impacts_only_unprotected_memory =
        memory_map_all_overlapping_regions_mprotected(map, info->mprotect.range,
                                                      false);
    if (impacts_only_unprotected_memory)
      return true;
    /* during init, we allow re-mprotecting memory, which we need to alter
    initially-RO destructors */
    else if (!memory_map_is_init_finished(map))
      return true;

    /* allow mprotecting memory that is already writable */
    uint32_t prot = memory_map_region_get_prot(map, info->mprotect.range);
    if (prot != MEMORY_MAP_PROT_INDETERMINATE && (prot & PROT_WRITE))
      return true;

    /* allow mprotecting memory to its current protection */
    if (prot == info->mprotect.prot)
      return true;

    break;
  }
  case EVENT_PKEY_MPROTECT: {
    /* allow mprotecting memory that has not been pkey_mprotected to our pkey */
    bool impacts_only_unprotected_memory =
        memory_map_all_overlapping_regions_pkey_mprotected(
            map, info->pkey_mprotect.range, false);
    bool sets_our_key =
        (info->pkey_mprotect.new_owner_pkey == info->pkey_mprotect.pkey);
    if (impacts_only_unprotected_memory && sets_our_key)
      return true;
    /* otherwise, only compartment 0 can pkey_mprotect anything as another
     * compartment */
    if (info->pkey_mprotect.pkey == 0 && !sets_our_key)
      return true;
    break;
  }
  case EVENT_CLONE: {
    return true;
    break;
  }
  case EVENT_EXEC: {
    return true;
    break;
  }
  case EVENT_NONE:
    return true;
    break;
  }
  return false;
}

/* update the memory map. returns whether the memory map could be updated as
 * requested */
static bool update_memory_map(struct memory_map *map, int event,
                              union event_info *info) {
  switch (event) {
  case EVENT_MMAP:
    if (info->mmap.flags & MAP_FIXED) {
      // mapping a fixed address is allowed to overlap/split existing regions
      if (!memory_map_split_region(map, info->mmap.range, info->mmap.pkey,
                                   info->mmap.prot)) {
        return memory_map_add_region(map, info->mmap.range, info->mmap.pkey,
                                     info->mmap.prot);
      } else {
        return true;
      }
    } else {
      return memory_map_add_region(map, info->mmap.range, info->mmap.pkey,
                                   info->mmap.prot);
    }
    break;
  case EVENT_MUNMAP:
    return memory_map_unmap_region(map, info->munmap.range);
    break;
  case EVENT_MREMAP: {
    uint32_t prot = memory_map_region_get_prot(map, info->mremap.old_range);
    if (prot == MEMORY_MAP_PROT_INDETERMINATE) {
      fprintf(stderr, "could not find prot for region to mremap\n");
      exit(1);
    }

    /* we don't need to handle MREMAP_MAYMOVE specially because we don't assume
    the old and new ranges have the same start */
    /* similarly, MREMAP_FIXED simply lets the request dictate the new range's
    start addr, about which we make no assumptions */
    if (info->mremap.flags & MREMAP_DONTUNMAP) {
      return memory_map_add_region(map, info->mremap.new_range,
                                   info->mremap.pkey, prot);
    } else {
      memory_map_unmap_region(map, info->mremap.old_range);
      return memory_map_add_region(map, info->mremap.new_range,
                                   info->mremap.pkey, prot);
    }
    break;
  }
  case EVENT_MADVISE:
    /* madvise does not modify the memory map state we care about here, but can
    clear memory contents with MADV_DONTNEED */
    return true;
    break;
  case EVENT_MPROTECT:
    return memory_map_mprotect_region(map, info->mprotect.range,
                                      info->mprotect.prot);
    break;
  case EVENT_PKEY_MPROTECT: {
    return memory_map_pkey_mprotect_region(map, info->mprotect.range,
                                           info->pkey_mprotect.new_owner_pkey);
    break;
  }
  case EVENT_CLONE: {
    return true;
    break;
  }
  case EVENT_EXEC: {
    memory_map_clear(map);
    return true;
    break;
  }
  case EVENT_NONE:
    return true;
    break;
  }
  return false;
}

#define PKEY_INVALID 255
#define PKRU(pkey) (~((3 << (2 * pkey)) | 3))

static unsigned char pkey_for_pkru(uint32_t pkru) {
#define CHECK(x) \
  case PKRU(x):  \
    return x;
  switch (pkru) {
    CHECK(0);
    CHECK(1);
    CHECK(2);
    CHECK(3);
    CHECK(4);
    CHECK(5);
    CHECK(6);
    CHECK(7);
    CHECK(8);
    CHECK(9);
    CHECK(10);
    CHECK(11);
    CHECK(12);
    CHECK(13);
    CHECK(14);
    CHECK(15);
  case 0x55555550:
    return 0;
  case 0x55555554:
    return 0;
  case 0:
    return 0;
  default:
    return PKEY_INVALID;
  }
#undef CHECK
}

/* Pass to mmap to signal end of program init */
#define IA2_FINISH_INIT_MAGIC 0x1a21face1a21faceULL

static bool event_marks_init_finished(enum mmap_event event, const union event_info *event_info) {
  return event == EVENT_MMAP &&
         event_info->mmap.range.start == IA2_FINISH_INIT_MAGIC &&
         event_info->mmap.flags & MAP_FIXED;
}

static void print_event(enum mmap_event event, const union event_info *event_info) {
  switch (event) {
  case EVENT_MMAP: {
    const struct mmap_info *info = &event_info->mmap;
    fprintf(stderr, "compartment %d mmap (%08zx, %zd, prot=%d, flags=%x, fd=%d)\n",
            info->pkey, info->range.start, info->range.len, info->prot,
            info->flags, info->fildes);
    break;
  }
  case EVENT_MUNMAP: {
    const struct munmap_info *info = &event_info->munmap;
    fprintf(stderr, "compartment %d munmap (%08zx, %zd)\n", info->pkey,
            info->range.start, info->range.len);
    break;
  }
  case EVENT_MREMAP: {
    const struct mremap_info *info = &event_info->mremap;
    fprintf(stderr, "compartment %d mremap (%08zx, %zd) to (%08zx, %zd)\n", info->pkey,
            info->old_range.start, info->old_range.len, info->new_range.start,
            info->new_range.len);
    break;
  }
  case EVENT_MADVISE: {
    const struct madvise_info *info = &event_info->madvise;
    fprintf(stderr, "compartment %d madvise (%08zx, %zd) with advice=%d\n", info->pkey,
            info->range.start, info->range.len, info->advice);
    break;
  }
  case EVENT_MPROTECT: {
    const struct mprotect_info *info = &event_info->mprotect;
    fprintf(stderr, "compartment %d mprotect (%08zx, %zd) to prot=%d\n", info->pkey,
            info->range.start, info->range.len, info->prot);
    break;
  }
  case EVENT_PKEY_MPROTECT: {
    const struct pkey_mprotect_info *info = &event_info->pkey_mprotect;
    fprintf(stderr, "compartment %d pkey_mprotect (%08zx, %zd) to %d prot=%d\n",
            info->pkey, info->range.start, info->range.len,
            info->new_owner_pkey, info->prot);
    break;
  }
  case EVENT_CLONE: {
    fprintf(stderr, "clone()\n");
    break;
  }
  case EVENT_EXEC: {
    fprintf(stderr, "exec()\n");
    break;
  }
  case EVENT_NONE: {
    fprintf(stderr, "untraced syscall\n");
    break;
  }
  }
}

/* query pid to determine the mmap-relevant event being requested. returns true
 * unless something horrible happens */
static bool interpret_syscall(struct user_regs_struct *regs, unsigned char pkey,
                              enum mmap_event *event, union event_info *event_info,
                              enum trace_mode mode) {
#ifdef __x86_64__
#define reg_pc regs->rip
#define reg_syscall regs->orig_rax
#define reg_retval regs->rax
#define reg_arg0 regs->rdi
#define reg_arg1 regs->rsi
#define reg_arg2 regs->rdx
#define reg_arg3 regs->r10
#define reg_arg4 regs->r8
#elif defined(__aarch64__)
#define reg_pc regs->pc
#define reg_syscall regs->regs[8]
#define reg_retval regs->regs[0]
#define reg_arg0 regs->regs[0]
#define reg_arg1 regs->regs[1]
#define reg_arg2 regs->regs[2]
#define reg_arg3 regs->regs[3]
#define reg_arg4 regs->regs[4]
#else
#error unsupported CPU architecture
#endif
  /* determine event from syscall # */
  unsigned long long syscall = reg_syscall;
  *event = event_from_syscall(syscall);

  /* dispatch on event and read args from registers.
  arg order is: rdi, rsi, rdx, r10, r8, r9 */
  switch (*event) {
  case EVENT_MMAP: {
    struct mmap_info *info = &event_info->mmap;
    info->range.start =
        reg_arg0; /* this will be replaced with the actual addr on return */
    info->range.len = reg_arg1;
    info->prot = reg_arg2;
    info->flags = reg_arg3;
    info->fildes = reg_arg4;
    info->pkey = pkey;
    break;
  }
  case EVENT_MUNMAP: {
    struct munmap_info *info = &event_info->munmap;
    info->range.start = reg_arg0;
    info->range.len = reg_arg1;
    info->pkey = pkey;
    break;
  }
  case EVENT_MREMAP: {
    struct mremap_info *info = &event_info->mremap;
    info->old_range.start = reg_arg0;
    info->old_range.len = reg_arg1;
    info->new_range.len = reg_arg2;
    info->flags = reg_arg3;

    if (info->flags & MREMAP_FIXED)
      info->new_range.start =
          reg_arg4; // accepts a 5th arg if this flag is present
    else
      info->new_range.start = info->old_range.start;

    info->pkey = pkey;
    break;
  }
  case EVENT_MADVISE: {
    struct madvise_info *info = &event_info->madvise;
    info->range.start = reg_arg0;
    info->range.len = reg_arg1;
    info->pkey = pkey;
    info->advice = reg_arg2;
    break;
  }
  case EVENT_MPROTECT: {
    struct mprotect_info *info = &event_info->mprotect;
    info->range.start = reg_arg0;
    info->range.len = reg_arg1;
    info->prot = reg_arg2;
    info->pkey = pkey;
    break;
  }
  case EVENT_PKEY_MPROTECT: {
    struct pkey_mprotect_info *info = &event_info->pkey_mprotect;
    info->range.start = reg_arg0;
    info->range.len = reg_arg1;
    info->prot = reg_arg2;
    info->new_owner_pkey = reg_arg3;
    info->pkey = pkey;
    break;
  }
  case EVENT_CLONE: {
    break;
  }
  case EVENT_EXEC: {
    break;
  }
  case EVENT_NONE: {
    /* when ptracing alone, this may occur; when we are a seccomp helper, this
    should not happen */
    if (mode == TRACE_MODE_SECCOMP) {
      fprintf(stderr, "seccomp tracer stopped on unexpected syscall (%llu)\n", syscall);
      return false;
    }
    break;
  }
  }

#ifdef DEBUG
  print_event(*event, event_info);
#endif

  return true;
}

/* returns whether the syscall succeeded (returned non-negative) */
static bool update_event_with_result(struct user_regs_struct *regs,
                                     enum mmap_event event,
                                     union event_info *event_info) {
  if ((int64_t)reg_retval < 0) {
    return false;
  }
  /* if mremap(MREMAP_MAYMOVE) or regular mmap() sans MAP_FIXED, we need to
  find out what addr came back */

  switch (event) {
  case EVENT_MMAP: {
    /* read result from registers */
    struct mmap_info *info = &event_info->mmap;
    debug_event_update("new start = %08llx\n", reg_retval);
    info->range.start = reg_retval;
    break;
  }
  case EVENT_MREMAP: {
    /* read result from registers */
    struct mremap_info *info = &event_info->mremap;
    debug_event_update("new start = %08llx\n", reg_retval);
    info->new_range.start = reg_retval;
    break;
  }
  default: {
    break;
  }
  }
  return true;
}

enum control_flow {
  RETURN_TRUE,
  RETURN_FALSE,
  CONTINUE,
};

#define propagate(control_flow) \
  switch (control_flow) {       \
  case RETURN_TRUE:             \
    return true;                \
  case RETURN_FALSE:            \
    return false;               \
  case CONTINUE:                \
    break;                      \
  }

enum wait_trap_result {
  WAIT_SYSCALL,
  WAIT_STOP,
  WAIT_GROUP_STOP,
  WAIT_EXITED,
  WAIT_SIGNALED,
  WAIT_SIGSEGV,
  WAIT_SIGCHLD,
  WAIT_ERROR,
  WAIT_PTRACE_CLONE,
  WAIT_PTRACE_FORK,
  WAIT_EXEC,
  WAIT_CONT,
};

/* wait for the next trap from the inferior.

returns the wait_trap_result corresponding to the event.

if the exit is WAIT_EXITED, the exit status will be placed in *exit_status_out
if it is non-NULL. */
static enum wait_trap_result wait_for_next_trap(pid_t pid, pid_t *pid_out, int *exit_status_out) {
  bool entry = (pid == -1);
  int stat = 0;
  static pid_t last_pid = 0; /* used to limit logs to when pid changes */
  pid_t waited_pid = waitpid(pid, &stat, __WALL);
  if (pid_out)
    *pid_out = waited_pid;
  if (last_pid != waited_pid) {
    debug_wait("waited, pid=%d\n", waited_pid);
    last_pid = waited_pid;
  }
  if (waited_pid < 0) {
    perror("waitpid");
    return WAIT_ERROR;
  }
  if (WIFEXITED(stat)) {
    if (exit_status_out)
      *exit_status_out = WEXITSTATUS(stat);
    return WAIT_EXITED;
  }
  if (WIFSIGNALED(stat)) {
    fprintf(stderr, "inferior killed by signal %d\n", WTERMSIG(stat));
    return WAIT_SIGNALED;
  }
  if (WIFSTOPPED(stat)) {
    /* stopped by a signal or by ptrace/seccomp which shows up as SIGTRAP */
    /* the next 8 bits of status give the ptrace event, if any */
    int ptrace_event = stat >> 16;
    if (ptrace_event > 0) {
      if (ptrace_event == PTRACE_EVENT_CLONE) {
        return WAIT_PTRACE_CLONE;
      }
      if (ptrace_event == PTRACE_EVENT_FORK) {
        return WAIT_PTRACE_FORK;
      }
      if (ptrace_event == PTRACE_EVENT_SECCOMP) {
        return WAIT_SYSCALL;
      }
      if (ptrace_event == PTRACE_EVENT_EXEC) {
        debug_event("exec event\n");
        return WAIT_EXEC;
      }
      if (ptrace_event == PTRACE_EVENT_STOP) {
        debug_event("stop event\n");
        return WAIT_STOP;
      }
      fprintf(stderr, "unknown ptrace event %d\n", ptrace_event);
    }
    switch (WSTOPSIG(stat)) {
    case SIGTRAP | 0x80:
      debug_event("child stopped by syscall %s\n", entry ? "entry" : "exit");
      return WAIT_SYSCALL;
    case SIGSTOP:
    case SIGTSTP:
    case SIGTTIN:
    case SIGTTOU:
      debug_event("possibly group stop\n");
      siginfo_t siginfo = {0};
      if (ptrace(PTRACE_GETSIGINFO, waited_pid, 0, &siginfo) < 0) {
        if (errno == EINVAL) {
          debug_event("child in group-stop\n");
          return WAIT_GROUP_STOP;
        }
      } else {
        debug_event("child stopped by SIGSTOP\n");
      }
      return WAIT_STOP;
    case SIGCONT:
      debug_event("child hit SIGCONT\n");
      return WAIT_CONT;
    case SIGCHLD:
      debug_event("child stopped by sigchld\n");
      return WAIT_SIGCHLD;
    case SIGSEGV:
      debug_event("child stopped by sigsegv\n");
      return WAIT_SIGSEGV;
    default:
      fprintf(stderr, "child stopped by unexpected signal %d\n", WSTOPSIG(stat));
      return WAIT_ERROR;
    }
  }
  fprintf(stderr, "unknown wait status %x\n", stat);
  return WAIT_ERROR;
}

long get_regs(pid_t pid, struct user_regs_struct *regs) {
#ifdef __x86_64__
  return ptrace(PTRACE_GETREGS, pid, 0, regs);
#elif defined(__aarch64__)
  struct iovec iov;
  iov.iov_base = (void *)&regs;
  iov.iov_len = sizeof(regs);
  return ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iov);
#else
#error unsupported CPU architecture
#endif
}

long set_regs(pid_t pid, struct user_regs_struct *regs) {
#ifdef __x86_64__
  return ptrace(PTRACE_SETREGS, pid, 0, regs);
#elif defined(__aarch64__)
  struct iovec iov;
  iov.iov_base = (void *)&regs;
  iov.iov_len = sizeof(regs);
  return ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &iov);
#else
#error unsupported CPU architecture
#endif
}

static void return_syscall_eperm(pid_t pid) {
  struct user_regs_struct regs_storage = {0};
  struct user_regs_struct *regs = &regs_storage;
  if (get_regs(pid, regs) < 0) {
    perror("could not PTRACE_GETREGS");
    return;
  }

  /* set to invalid syscall */
  reg_syscall = -1;
  set_regs(pid, regs);
  debug_forbid("set syscall # to -1\n");

  /* run syscall until exit */
  ptrace(PTRACE_SYSCALL, pid, 0, 0);
  waitpid(pid, NULL, __WALL);
  debug_forbid("continued\n");

  if (get_regs(pid, regs) < 0) {
    perror("could not PTRACE_GETREGS");
    return;
  }
  /* return -EPERM */
  reg_retval = -EPERM;
  set_regs(pid, regs);
  debug_forbid("wrote -eperm to rax\n");
}

/* a memory map that tracks multiple threads under the same process */
struct memory_map_for_process {
  struct memory_map *map;
  pid_t *pids;
  size_t n_pids;
};

/* the set of memory maps for a family of traced processes */
struct memory_maps {
  struct memory_map_for_process *maps_for_processes;
  size_t n_maps;
};

static struct memory_map_for_process *find_memory_map(struct memory_maps *maps, pid_t pid) {
  for (int i = 0; i < maps->n_maps; i++) {
    struct memory_map_for_process *map_for_proc = &maps->maps_for_processes[i];
    for (int j = 0; j < map_for_proc->n_pids; j++) {
      if (map_for_proc->pids[j] == pid) {
        return map_for_proc;
      }
    }
  }
  return NULL;
}

static void add_pid(struct memory_map_for_process *map_for_proc, pid_t pid) {
  map_for_proc->n_pids++;
  map_for_proc->pids = realloc(map_for_proc->pids, map_for_proc->n_pids * sizeof(pid_t));
  map_for_proc->pids[map_for_proc->n_pids - 1] = pid;
}

static bool remove_pid(struct memory_map_for_process *map_for_proc, pid_t pid) {
  for (int j = 0; j < map_for_proc->n_pids; j++) {
    if (map_for_proc->pids[j] == pid) {
      // swap last into its place and decrement count
      map_for_proc->pids[j] = map_for_proc->pids[map_for_proc->n_pids - 1];
      map_for_proc->n_pids--;
      return true;
    }
  }
  return false;
}

static void add_map(struct memory_maps *maps, struct memory_map_for_process map) {
  maps->n_maps++;
  maps->maps_for_processes = realloc(maps->maps_for_processes, maps->n_maps * sizeof(struct memory_map_for_process));
  maps->maps_for_processes[maps->n_maps - 1] = map;
}

static bool remove_map(struct memory_maps *maps, struct memory_map_for_process *map_to_remove) {
  for (int i = 0; i < maps->n_maps; i++) {
    struct memory_map_for_process *map_for_proc = &maps->maps_for_processes[i];
    if (map_for_proc == map_to_remove) {
      // swap last into its place and decrement count
      maps->maps_for_processes[i] = maps->maps_for_processes[maps->n_maps - 1];
      maps->n_maps--;
      return true;
    }
  }
  return false;
}

static struct memory_map_for_process for_process_new(struct memory_map *map, pid_t pid) {
  pid_t *pids = malloc(sizeof(pid_t));
  pids[0] = pid;
  struct memory_map_for_process for_processes = {.map = map, .pids = pids, .n_pids = 1};
  return for_processes;
}

static enum control_flow handle_process_exit(struct memory_maps *maps, pid_t waited_pid) {
  struct memory_map_for_process *map_for_proc = find_memory_map(maps, waited_pid);
  if (!map_for_proc) {
    fprintf(stderr, "exited: could not find memory map for process %d\n", waited_pid);
    return RETURN_FALSE;
  }
  if (!remove_pid(map_for_proc, waited_pid)) {
    fprintf(stderr, "could not remove pid %d from memory map\n", waited_pid);
    return RETURN_FALSE;
  }
  if (map_for_proc->n_pids == 0) {
    if (!remove_map(maps, map_for_proc)) {
      fprintf(stderr, "could not remove memory map for pid %d\n", waited_pid);
    }
  }

  // if all maps are gone, exit
  if (maps->n_maps == 0) {
    return RETURN_TRUE;
  }
  return CONTINUE;
}

/* track the inferior process' memory map.

returns true if the inferior exits, false on trace error.

if true is returned, the inferior's exit status will be stored to *exit_status_out if not NULL. */
bool track_memory_map(pid_t pid, int *exit_status_out, enum trace_mode mode) {

  struct memory_map *map = memory_map_new();
  struct memory_map_for_process *for_process = malloc(sizeof(struct memory_map_for_process));
  *for_process = for_process_new(map, pid);

  struct memory_maps maps = {
      .maps_for_processes = for_process,
      .n_maps = 1,
  };

  enum __ptrace_request continue_request = mode == TRACE_MODE_PTRACE_SYSCALL ? PTRACE_SYSCALL : PTRACE_CONT;
  while (true) {
    /* wait for the process to get signalled */
    pid_t waited_pid = pid;
    enum wait_trap_result wait_result = wait_for_next_trap(-1, &waited_pid, exit_status_out);
    switch (wait_result) {
    /* we need to handle events relating to process lifetime upfront: these
    include clone()/fork()/exec() and sigchld */
    case WAIT_CONT: {
      if (ptrace(continue_request, waited_pid, 0, SIGCONT) < 0) {
        perror("could not PTRACE_SYSCALL...");
      }
      continue;
    }
    case WAIT_STOP: {
      if (ptrace(continue_request, waited_pid, 0, SIGSTOP) < 0) {
        perror("could not PTRACE_SYSCALL...");
      }
      continue;
      break;
    }
    case WAIT_SIGCHLD:
      if (ptrace(continue_request, waited_pid, 0, SIGCHLD) < 0) {
        perror("could not PTRACE_SYSCALL...");
      }
      continue;
      break;
    case WAIT_SIGSEGV:
      if (ptrace(continue_request, waited_pid, 0, SIGSEGV) < 0) {
        perror("could not PTRACE_SYSCALL...");
      }
      continue;
      break;
    case WAIT_GROUP_STOP:
      printf("group stop in syscall entry\n");
      if (ptrace(PTRACE_LISTEN, waited_pid, 0, 0) < 0) {
        perror("could not PTRACE_LISTEN...");
      }
      continue;
      break;
    case WAIT_SYSCALL:
      break;
    case WAIT_ERROR: {
      struct user_regs_struct regs_storage = {0};
      struct user_regs_struct *regs = &regs_storage;
      if (get_regs(waited_pid, regs) < 0) {
        perror("could not PTRACE_GETREGS");
        return false;
      }
      fprintf(stderr, "error at rip=%p\n", (void *)reg_pc);
      for (int i = 0; i < maps.n_maps; i++) {
        for (int j = 0; j < maps.maps_for_processes[i].n_pids; j++) {
          pid_t pid = maps.maps_for_processes[i].pids[j];
          kill(pid, SIGSTOP);
          ptrace(PTRACE_DETACH, pid, 0, 0);
        }
      }
      return false;
    }
    case WAIT_PTRACE_CLONE: {
      pid_t cloned_pid = 0;
      int ret = ptrace(PTRACE_GETEVENTMSG, waited_pid, 0, &cloned_pid);
      if (ret < 0) {
        perror("ptrace(PTRACE_GETEVENTMSG) upon clone");
        return WAIT_ERROR;
      }
      debug_proc("should track child pid %d\n", cloned_pid);

      struct memory_map_for_process *map_for_proc = find_memory_map(&maps, waited_pid);
      add_pid(map_for_proc, cloned_pid);
      break;
    }
    case WAIT_PTRACE_FORK: {
      pid_t cloned_pid = 0;
      int ret = ptrace(PTRACE_GETEVENTMSG, waited_pid, 0, &cloned_pid);
      if (ret < 0) {
        perror("ptrace(PTRACE_GETEVENTMSG) upon fork");
        return WAIT_ERROR;
      }
      debug_proc("should track forked child pid %d\n", cloned_pid);

      struct memory_map_for_process *map_for_proc = find_memory_map(&maps, waited_pid);
      struct memory_map *cloned = memory_map_clone(map_for_proc->map);

      remove_pid(map_for_proc, cloned_pid);
      add_map(&maps, for_process_new(cloned, cloned_pid));
      break;
    }
    case WAIT_EXEC: {
      struct memory_map_for_process *map_for_proc = find_memory_map(&maps, waited_pid);
      if (!map_for_proc) {
        fprintf(stderr, "exec: could not find memory map for process %d\n", waited_pid);
        return false;
      }
      struct memory_map *map = map_for_proc->map;
      memory_map_clear(map);
      break;
    }
    case WAIT_SIGNALED: {
      fprintf(stderr, "process received fatal signal (syscall entry)\n");
      enum control_flow cf = handle_process_exit(&maps, waited_pid);
      return false;
    }
    case WAIT_EXITED: {
      debug_exit("pid %d exited (syscall entry)\n", waited_pid);
      propagate(handle_process_exit(&maps, waited_pid));

      // in any case, this process is gone, so wait for a new one
      continue;
    }
    }

    struct memory_map_for_process *map_for_proc = find_memory_map(&maps, waited_pid);
    if (!map_for_proc) {
      fprintf(stderr, "could not find memory map for process %d\n", waited_pid);
      return false;
    }

    struct memory_map *map = map_for_proc->map;

    /* read which syscall is being called and its args */
    struct user_regs_struct regs_storage = {0};
    struct user_regs_struct *regs = &regs_storage;
    if (get_regs(waited_pid, regs) < 0) {
      perror("could not PTRACE_GETREGS");
      return false;
    }

    /* if syscall number is -1, finish and kill process */
    if (reg_syscall == -1) {
      return false;
    }

#ifdef __x86_64__
    /* read pkru */
    uint32_t pkru = -1;
    bool res = get_inferior_pkru(waited_pid, &pkru);
    if (!res) {
      fprintf(stderr, "could not get pkey\n");
      return false;
    }
    unsigned char pkey = pkey_for_pkru(pkru);
    if (pkey == PKEY_INVALID) {
      fprintf(stderr, "pkru value %08x does not correspond to any pkey!\n",
              pkru);
      return false;
    }
#elif defined(__aarch64__)
    /* read compartment tag from x18 */
    unsigned char pkey = regs->regs[18];
#else
#error unsupported CPU architecture
#endif

    union event_info event_info = {0};
    enum mmap_event event = EVENT_NONE;
    if (!interpret_syscall(regs, pkey, &event, &event_info, mode)) {
      fprintf(stderr, "could not interpret syscall!\n");
      return false;
    }

    /* pick up signal marking IA2 init finished to start forbidding init-only operations */
    if (event_marks_init_finished(event, &event_info)) {
      if (!memory_map_mark_init_finished(map)) {
        fprintf(stderr, "attempting to re-finish init! (rip=%p)\n", (void *)reg_pc);
        return false;
      }
      debug_op("init finished\n");
      /* finish syscall; it will fail benignly */
      if (ptrace(PTRACE_SYSCALL, waited_pid, 0, 0) < 0) {
        perror("could not PTRACE_SYSCALL");
      }
      switch (wait_for_next_trap(waited_pid, NULL, exit_status_out)) {
      case WAIT_SYSCALL:
        break;
      case WAIT_ERROR:
        return false;
      case WAIT_SIGCHLD:
        break;
      default:
        return false;
      }
    } else if (!is_op_permitted(map, event, &event_info)) {
      fprintf(stderr, "forbidden operation requested: ");
      print_event(event, &event_info);
      const struct range *range = event_target_range(event, &event_info);
      if (range != NULL) {
        printf("region pkey: %d\n", memory_map_region_get_owner_pkey(map, *range));
      }
      return_syscall_eperm(waited_pid);
      if (ptrace(continue_request, waited_pid, 0, 0) < 0) {
        perror("could not PTRACE_SYSCALL");
        return false;
      }
      continue;
    } else {
      debug_policy("operation allowed: %s (syscall %lld)\n", event_name(event),
                   reg_syscall);
    }

  // if we are in TRACE_MODE_PTRACE_SYSCALL, we will see EXITED/PTRACE_CLONE here
  syscall_exit:
    /* run the actual syscall until syscall exit so we can read its result */
    if (ptrace(PTRACE_SYSCALL, waited_pid, 0, 0) < 0) {
      perror("could not PTRACE_SYSCALL");
    }
    switch (wait_for_next_trap(waited_pid, NULL, exit_status_out)) {
    case WAIT_STOP:
      break;
    case WAIT_SYSCALL:
      break;
    case WAIT_ERROR:
      return false;
    case WAIT_SIGCHLD:
      break;
    case WAIT_PTRACE_CLONE: {
      pid_t cloned_pid = 0;
      int ret = ptrace(PTRACE_GETEVENTMSG, waited_pid, 0, &cloned_pid);
      if (ret < 0) {
        perror("ptrace(PTRACE_GETEVENTMSG) upon clone");
        return WAIT_ERROR;
      }
      debug_proc("syscall exit; should track child pid %d\n", cloned_pid);

      struct memory_map_for_process *map_for_proc = find_memory_map(&maps, waited_pid);
      map_for_proc->n_pids++;
      map_for_proc->pids = realloc(map_for_proc->pids, map_for_proc->n_pids * sizeof(pid_t));
      map_for_proc->pids[map_for_proc->n_pids - 1] = cloned_pid;
      break;
      goto syscall_exit;
    }
    case WAIT_EXITED:
      debug_exit("pid %d exited (syscall exit)\n", waited_pid);
      propagate(handle_process_exit(&maps, waited_pid));

      // in any case, this process is gone, so wait for a new one
      continue;
    case WAIT_EXEC:
      fprintf(stderr, "unexpected PTRACE_O_TRACEEXEC stop at syscall exit\n");
      struct memory_map_for_process *map_for_proc = find_memory_map(&maps, waited_pid);
      if (!map_for_proc) {
        fprintf(stderr, "exec: could not find memory map for process %d\n", waited_pid);
        return false;
      }
      struct memory_map *map = map_for_proc->map;
      memory_map_clear(map);

      /* retry ptrace because it was not actually syscall exit this time around */
      goto syscall_exit;
    default:
      printf("unexpected wait result on syscall exit\n");
      return true;
    }

    /* read syscall result from registers */
    if (get_regs(waited_pid, regs) < 0) {
      perror("could not PTRACE_GETREGS");
      return false;
    }

    /* if syscall succeeded, update event */
    if (update_event_with_result(regs, event, &event_info)) {
      /* track effect of syscall on memory map */
      if (!update_memory_map(map, event, &event_info)) {
        fprintf(stderr, "could not update memory map! (operation=%s, rip=%p)\n", event_name(event), (void *)reg_pc);
        return false;
      }
    }

    /* run until the next syscall entry or traced syscall (depending on mode) */
    if (ptrace(continue_request, waited_pid, 0, 0) < 0) {
      perror("could not PTRACE_SYSCALL...");
    }
  }
}
