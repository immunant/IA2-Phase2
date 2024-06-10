#pragma once

#include "memory_map.h"

struct mmap_info {
  struct range range;
  int prot;
  int flags;
  int fildes;
  unsigned char pkey;
};

struct munmap_info {
  struct range range;
  unsigned char pkey;
};

struct mremap_info {
  struct range old_range;
  struct range new_range;
  int flags;
  unsigned char pkey;
};

struct madvise_info {
  struct range range;
  int advice;
  unsigned char pkey;
};

struct mprotect_info {
  struct range range;
  int prot;
  unsigned char pkey;
};

struct pkey_mprotect_info {
  struct range range;
  int prot;
  unsigned char new_owner_pkey;
  unsigned char pkey;
};

union event_info {
  struct mmap_info mmap;
  struct munmap_info munmap;
  struct mremap_info mremap;
  struct madvise_info madvise;
  struct mprotect_info mprotect;
  struct pkey_mprotect_info pkey_mprotect;
};

enum mmap_event {
  EVENT_MMAP,
  EVENT_MUNMAP,
  EVENT_MREMAP,
  EVENT_MADVISE,
  EVENT_MPROTECT,
  EVENT_PKEY_MPROTECT,
  EVENT_CLONE,
  EVENT_EXEC,
  EVENT_NONE,
};

static const char *event_names[] = {
    "MMAP",
    "MUNMAP",
    "MREMAP",
    "MADVISE",
    "MPROTECT",
    "PKEY_MPROTECT",
    "CLONE",
    "EXEC",
    "NONE",
};

static const char *event_name(enum mmap_event event) {
  return event_names[event];
}

static inline const struct range *event_target_range(enum mmap_event event, const union event_info *info) {
  switch (event) {
  case EVENT_MMAP:
    return &info->mmap.range;
  case EVENT_MUNMAP:
    return &info->munmap.range;
  case EVENT_MREMAP:
    return &info->mremap.old_range;
  case EVENT_MADVISE:
    return &info->madvise.range;
  case EVENT_MPROTECT:
    return &info->mprotect.range;
  case EVENT_PKEY_MPROTECT:
    return &info->pkey_mprotect.range;
  case EVENT_CLONE:
    return NULL;
  case EVENT_EXEC:
    return NULL;
  case EVENT_NONE:
    return NULL;
    break;
  }
}

enum mmap_event event_from_syscall(uint64_t syscall_nr);
