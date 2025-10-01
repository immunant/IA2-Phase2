#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "ia2_destructor_runtime.h"

#include "ia2.h"
#include "ia2_internal.h"

#include <dlfcn.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct ia2_destructor_runtime_entry {
  void (*wrapper)(void);
  const struct ia2_destructor_metadata_record *record;
  uint32_t pkru_value;
};

static struct ia2_destructor_runtime_entry *destructor_entries;
static size_t destructor_entry_count;
static bool initialized;

static uint32_t compute_union_pkru(int exit_pkey, int target_pkey) {
  uint32_t mask = ~0u;
  mask &= ~0b11u; // Always allow shared key 0.
  mask &= ~(0b11u << (2 * exit_pkey));
  mask &= ~(0b11u << (2 * target_pkey));
  return mask;
}

static void ensure_initialized(void) {
  if (initialized) {
    return;
  }
  initialized = true;

  if (ia2_destructor_metadata_count == 0) {
    return;
  }

  destructor_entries =
      calloc(ia2_destructor_metadata_count, sizeof(struct ia2_destructor_runtime_entry));
  if (!destructor_entries) {
    fprintf(stderr, "ia2: failed to allocate destructor metadata cache\n");
    return;
  }

  for (unsigned int i = 0; i < ia2_destructor_metadata_count; ++i) {
    const struct ia2_destructor_metadata_record *record = &ia2_destructor_metadata[i];

    dlerror();
    void *symbol = dlsym(RTLD_DEFAULT, record->wrapper);
    const char *dlerr = dlerror();
    if (dlerr != NULL || symbol == NULL) {
      fprintf(stderr, "ia2: failed to resolve destructor wrapper '%s': %s\n",
              record->wrapper, dlerr ? dlerr : "symbol not found");
      continue;
    }

    struct ia2_destructor_runtime_entry *entry = &destructor_entries[destructor_entry_count++];
    entry->wrapper = (void (*)(void))symbol;
    entry->record = record;
    if (record->uses_union_pkru) {
      entry->pkru_value = compute_union_pkru(ia2_destructor_exit_pkey, record->compartment_pkey);
    } else {
      entry->pkru_value = PKRU(record->compartment_pkey);
    }
  }
}

void ia2_destructor_runtime_init(void) {
  ensure_initialized();
}

bool ia2_destructor_metadata_lookup(void (*wrapper)(void),
                                    const struct ia2_destructor_metadata_record **out_record,
                                    uint32_t *out_pkru_value) {
  ensure_initialized();

  if (!wrapper || destructor_entry_count == 0) {
    return false;
  }

  for (size_t i = 0; i < destructor_entry_count; ++i) {
    if (destructor_entries[i].wrapper == wrapper) {
      if (out_record) {
        *out_record = destructor_entries[i].record;
      }
      if (out_pkru_value) {
        *out_pkru_value = destructor_entries[i].pkru_value;
      }
      return true;
    }
  }
  return false;
}

uint32_t ia2_destructor_pkru_for(void (*wrapper)(void), int target_compartment_pkey) {
  uint32_t pkru_value;
  if (ia2_destructor_metadata_lookup(wrapper, NULL, &pkru_value)) {
    return pkru_value;
  }
  return PKRU(target_compartment_pkey);
}

// Helper to read/write PKRU, guarded for non-x86 architectures.
#if defined(__x86_64__)
static inline uint32_t read_pkru(void) {
  uint32_t pkru;
  __asm__ volatile("xor %%ecx, %%ecx\n"
                   "rdpkru\n"
                   : "=a"(pkru)
                   :
                   : "ecx", "edx");
  return pkru;
}

static inline void write_pkru(uint32_t pkru) {
  __asm__ volatile("xor %%ecx, %%ecx\n"
                   "xor %%edx, %%edx\n"
                   "wrpkru\n"
                   :
                   : "a"(pkru)
                   : "ecx", "edx");
}
#endif

uint32_t ia2_destructor_enter(void *wrapper_addr, int target_pkey) {
#if defined(__x86_64__)
  uint32_t original_pkru = read_pkru();
  uint32_t desired_pkru = ia2_destructor_pkru_for((void (*)(void))wrapper_addr, target_pkey);
  if (desired_pkru != original_pkru) {
    write_pkru(desired_pkru);
#ifdef IA2_TRACE_EXIT
    ia2_trace_exit_record((int)ia2_get_compartment(), target_pkey, desired_pkru);
#endif
  }
  return original_pkru;
#else
  (void)wrapper_addr;
  (void)target_pkey;
  return 0;
#endif
}

void ia2_destructor_leave(uint32_t original_pkru) {
#if defined(__x86_64__)
  write_pkru(original_pkru);
#else
  (void)original_pkru;
#endif
}
