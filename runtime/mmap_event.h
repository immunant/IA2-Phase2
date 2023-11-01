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
  struct mprotect_info mprotect;
  struct pkey_mprotect_info pkey_mprotect;
};

enum mmap_event {
  EVENT_MMAP,
  EVENT_MUNMAP,
  EVENT_MREMAP,
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
    "MPROTECT",
    "PKEY_MPROTECT",
    "CLONE",
    "EXEC",
    "NONE",
};

static const char *event_name(enum mmap_event event) {
  return event_names[event];
}

enum mmap_event event_from_syscall(uint64_t rax);
