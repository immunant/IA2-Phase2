#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

#include "get_inferior_pkru.h"
#include "memory_map.h"
#include "mmap_event.h"

#ifdef DEBUG
#define debug(...) fprintf(stderr, __VA_ARGS__)
#define debug_op(...) fprintf(stderr, __VA_ARGS__)
#define debug_policy(...) fprintf(stderr, __VA_ARGS__)
#define debug_event_update(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug(...)
#define debug_op(...)
#define debug_policy(...)
#define debug_event_update(...)
#endif

bool is_op_permitted(struct memory_map *map, int event,
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
  case EVENT_MPROTECT: {
    /* allow mprotecting memory that has not been mprotected */
    bool impacts_only_unprotected_memory =
        memory_map_all_overlapping_regions_mprotected(map, info->mprotect.range,
                                                      false);
    if (impacts_only_unprotected_memory)
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
  case EVENT_NONE:
    return true;
    break;
  }
  return false;
}

/* update the memory map. returns whether the memory map could be updated as
 * requested */
bool update_memory_map(struct memory_map *map, int event,
                       union event_info *info) {
  switch (event) {
  case EVENT_MMAP:
    if (info->mmap.flags & MAP_FIXED) {
      // mapping a fixed address is allowed to overlap/split existing regions
      if (!memory_map_split_region(map, info->mmap.range, info->mmap.pkey,
                                   info->mmap.prot)) {
        fprintf(stderr, "no split, adding region\n");
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
  case EVENT_MPROTECT:
    return memory_map_mprotect_region(map, info->mprotect.range,
                                      info->mprotect.prot);
    break;
  case EVENT_PKEY_MPROTECT: {
    return memory_map_pkey_mprotect_region(map, info->mprotect.range,
                                           info->pkey_mprotect.new_owner_pkey);
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

unsigned char pkey_for_pkru(uint32_t pkru) {
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

/* query pid to determine the mmap-relevant event being requested. returns true
 * unless something horrible happens */
bool interpret_syscall(struct user_regs_struct *regs, unsigned char pkey,
                       enum mmap_event *event, union event_info *event_info) {
  /* determine event from syscall # */
  *event = event_from_syscall(regs->orig_rax);

  debug_op("event: %s\n", event_names[*event]);

  /* dispatch on event and read args from registers.
  arg order is: rdi, rsi, rdx, r10, r8, r9 */
  switch (*event) {
  case EVENT_MMAP: {
    struct mmap_info *info = &event_info->mmap;
    info->range.start =
        regs->rdi; /* this will be replaced with the actual addr on return */
    info->range.len = regs->rsi;
    info->prot = regs->rdx;
    info->flags = regs->r10;
    info->fildes = regs->r8;
    info->pkey = pkey;

    debug_op("compartment %d mmap (%08zx, %zd, prot=%d, flags=%x, fd=%d)\n",
             info->pkey, info->range.start, info->range.len, info->prot,
             info->flags, info->fildes);
    break;
  }
  case EVENT_MUNMAP: {
    struct munmap_info *info = &event_info->munmap;
    info->range.start = regs->rdi;
    info->range.len = regs->rsi;
    info->pkey = pkey;

    debug_op("compartment %d munmap (%08zx, %zd)\n", info->pkey,
             info->range.start, info->range.len);
    break;
  }
  case EVENT_MREMAP: {
    struct mremap_info *info = &event_info->mremap;
    info->old_range.start = regs->rdi;
    info->old_range.len = regs->rsi;
    info->new_range.len = regs->rdx;
    info->flags = regs->r10;

    if (info->flags & MREMAP_FIXED)
      info->new_range.start =
          regs->r8; // accepts a 5th arg if this flag is present
    else
      info->new_range.start = info->old_range.start;

    info->pkey = pkey;

    debug_op("compartment %d mremap (%08zx, %zd) to (%08zx, %zd)\n", info->pkey,
             info->old_range.start, info->old_range.len, info->new_range.start,
             info->new_range.len);
    break;
  }
  case EVENT_MPROTECT: {
    struct mprotect_info *info = &event_info->mprotect;
    info->range.start = regs->rdi;
    info->range.len = regs->rsi;
    info->prot = regs->rdx;
    info->pkey = pkey;

    debug_op("compartment %d mprotect (%08zx, %zd) to prot=%d\n", info->pkey,
             info->range.start, info->range.len, info->prot);
    break;
  }
  case EVENT_PKEY_MPROTECT: {
    struct pkey_mprotect_info *info = &event_info->pkey_mprotect;
    info->range.start = regs->rdi;
    info->range.len = regs->rsi;
    info->prot = regs->rdx;
    info->new_owner_pkey = regs->r10;
    info->pkey = pkey;

    debug_op("compartment %d pkey_mprotect (%08zx, %zd) to %d prot=%d\n",
             info->pkey, info->range.start, info->range.len,
             info->new_owner_pkey, info->prot);
    break;
  }
  case EVENT_NONE: {
    /* when ptracing alone, this may occur; when we are a seccomp helper, this
    should not happen */
    break;
  }
  }
  return true;
}

void update_event_with_result(struct user_regs_struct *regs,
                              enum mmap_event event,
                              union event_info *event_info) {
  /* if mremap(MREMAP_MAYMOVE) or regular mmap() sans MAP_FIXED, we need to
  find out what addr came back */

  switch (event) {
  case EVENT_MMAP: {
    /* read result from registers */
    struct mmap_info *info = &event_info->mmap;
    debug_event_update("new start = %08zx\n", regs->rax);
    info->range.start = regs->rax;
    break;
  }
  case EVENT_MREMAP: {
    /* read result from registers */
    struct mmap_info *info = &event_info->mmap;
    debug_event_update("new start = %08zx\n", regs->rax);
    info->range.start = regs->rax;
    break;
  }
  default: {
    break;
  }
  }
}

/* returns true if an error occurred or the process exited */
bool wait_for_next_trap(pid_t pid) {
  int stat = 0;
  int ret = waitpid(pid, &stat, 0);
  if (ret < 0) {
    perror("waitpid");
    return true;
  }
  if (WIFEXITED(stat)) {
    fprintf(stderr, "inferior exited\n");
    return true;
  }
  if (WIFSIGNALED(stat)) {
    fprintf(stderr, "inferior killed by signal\n");
    return true;
  }
  return false;
}

void return_syscall_eperm(pid_t pid) {
  struct user_regs_struct regs = {0};
  if (ptrace(PTRACE_GETREGS, pid, 0, &regs) < 0) {
    perror("could not PTRACE_GETREGS");
    return;
  }

  /* set to invalid syscall */
  regs.orig_rax = -1;
  ptrace(PTRACE_SETREGS, pid, 0, &regs);
  fprintf(stderr, "set syscall # to -1\n");

  /* run syscall until exit */
  ptrace(PTRACE_SYSCALL, pid, 0, 0);
  waitpid(pid, NULL, 0);
  fprintf(stderr, "continued\n");

  if (ptrace(PTRACE_GETREGS, pid, 0, &regs) < 0) {
    perror("could not PTRACE_GETREGS");
    return;
  }
  /* return -EPERM */
  regs.rax = -EPERM;
  ptrace(PTRACE_SETREGS, pid, 0, &regs);
  fprintf(stderr, "wrote -eperm to rax\n");
}

void track_memory_map(pid_t pid, struct memory_map *map) {
  while (true) {
    /* run until the next syscall entry */
    if (ptrace(PTRACE_SYSCALL, pid, 0, 0) < 0) {
      perror("could not PTRACE_SYSCALL");
    }
    /* wait for the process to get signalled */
    if (wait_for_next_trap(pid))
      return;

    /* read which syscall is being called and its args */
    struct user_regs_struct regs = {0};
    if (ptrace(PTRACE_GETREGS, pid, 0, &regs) < 0) {
      perror("could not PTRACE_GETREGS");
      return;
    }

    /* if syscall number is -1, finish and kill process */
    if (regs.orig_rax == -1) {
      return;
    }

    /* read pkru */
    uint32_t pkru = -1;
    bool res = get_inferior_pkru(pid, &pkru);
    if (!res) {
      fprintf(stderr, "could not get pkey\n");
      return;
    }
    unsigned char pkey = pkey_for_pkru(pkru);
    if (pkey == PKEY_INVALID) {
      fprintf(stderr, "pkru value %08x does not correspond to any pkey!\n",
              pkru);
      return;
    }

    union event_info event_info = {0};
    enum mmap_event event = EVENT_NONE;
    if (!interpret_syscall(&regs, pkey, &event, &event_info)) {
      fprintf(stderr, "could not interpret syscall!\n");
    }

    if (!is_op_permitted(map, event, &event_info)) {
      fprintf(stderr, "forbidden operation requested: %s\n", event_name(event));
      return_syscall_eperm(pid);
      continue;
    } else {
      debug_policy("operation allowed: %s (syscall %lld)\n", event_name(event),
                   regs.orig_rax);
    }

    /* run the actual syscall until syscall exit so we can read its result */
    if (ptrace(PTRACE_SYSCALL, pid, 0, 0) < 0) {
      perror("could not PTRACE_SYSCALL");
    }
    if (wait_for_next_trap(pid))
      return;

    /* read syscall result from registers */
    if (ptrace(PTRACE_GETREGS, pid, 0, &regs) < 0) {
      perror("could not PTRACE_GETREGS");
      return;
    }

    /* update event */
    update_event_with_result(&regs, event, &event_info);

    /* track effect of syscall on memory map */
    if (!update_memory_map(map, event, &event_info)) {
      fprintf(stderr, "could not update memory map! (operation=%s, rip=%p)\n", event_name(event), (void *)regs.rip);
      return;
    }
  }
}
