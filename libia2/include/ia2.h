#pragma once

#include <assert.h>
#include <errno.h>
#include <link.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#ifdef LIBIA2_INSECURE
#define INIT_COMPARTMENT(n)
#else
#define INIT_COMPARTMENT(n) _INIT_COMPARTMENT(n)
#endif

/// Protect pages in the given shared object
///
/// \param info dynamic linker information for the current object
/// \param size size of \p info in bytes
/// \param data pointer to a PhdrSearchArgs structure
///
/// The callback passed to dl_iterate_phdr in the constructor inserted by
/// INIT_COMPARTMENT to pkey_mprotect the pages corresponding to the
/// compartment's loaded segments.
///
/// Iterates over shared objects until an object containing the address \p
/// data->address is found. Protect the pages in that object according to the
/// information in the search arguments.
int protect_pages(struct dl_phdr_info *info, size_t size, void *data);

// The data argument each time dl_iterate_phdr calls protect_pages
struct PhdrSearchArgs {
  // The compartment pkey to use when the segments are found
  int32_t pkey;
  // The address to search for while iterating through segments
  const void *address;
};

// Attribute for read-write variables that can be accessed from any untrusted
// compartments.
#define IA2_SHARED_DATA __attribute__((section("ia2_shared_data")))

// Initializes a compartment with protection key `n` when the ELF invoking this
// macro is loaded. This must only be called once for each key. The compartment
// includes all segments in the ELF except the `ia2_shared_data` section, if one
// exists.
#define _INIT_COMPARTMENT(n)                                                   \
  extern int ia2_n_pkeys_to_alloc;                                             \
  void ensure_pkeys_allocated(int *n_to_alloc);                                \
  __attribute__((constructor)) static void init_pkey_ctor() {                  \
    ensure_pkeys_allocated(&ia2_n_pkeys_to_alloc);                             \
    struct PhdrSearchArgs args = {                                             \
        .pkey = n,                                                             \
        .address = &init_pkey_ctor,                                            \
    };                                                                         \
    dl_iterate_phdr(protect_pages, &args);                                     \
  }

#define STACK_SIZE (4 * 1024 * 1024)

#ifdef LIBIA2_INSECURE
#define INIT_RUNTIME(n)                                                        \
  int ia2_n_pkeys_to_alloc = 0;                                                \
  void protect_stack(int i, char *stack) {}                                    \
  INIT_RUNTIME_COMMON(n)
#else
#define INIT_RUNTIME(n)                                                        \
  int ia2_n_pkeys_to_alloc = n;                                                \
  /* Protect a stack with the ith pkey. */                                     \
  void protect_stack(int i, char *stack) {                                     \
    if (!stack) {                                                              \
      exit(-1);                                                                \
    }                                                                          \
    int res = pkey_mprotect(stack, STACK_SIZE, PROT_READ | PROT_WRITE, i);     \
    if (res == -1) {                                                           \
      printf("Failed to mprotect stack %d (%d)\n", i, errno);                  \
    }                                                                          \
  }                                                                            \
  INIT_RUNTIME_COMMON(n)
#endif

#define INIT_RUNTIME_COMMON(n)                                                 \
  /* Allocate a fixed-size stack. */                                           \
  char *allocate_stack(int i) {                                                \
    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,       \
                               MAP_PRIVATE | MAP_ANON, -1, 0);                 \
    if (stack == MAP_FAILED) {                                                 \
      printf("Failed to allocate stack %d (%d)\n", i, errno);                  \
      exit(-1);                                                                \
    }                                                                          \
    return stack;                                                              \
  }                                                                            \
  /* Ensure that all required pkeys are allocated. */                          \
  void ensure_pkeys_allocated(int *n_to_alloc) {                               \
    if (*n_to_alloc != 0) {                                                    \
      for (int pkey = 1; pkey <= *n_to_alloc; pkey++) {                        \
        int allocated = pkey_alloc(0, 0);                                      \
        if (allocated != pkey) {                                               \
          printf(                                                              \
              "Failed to allocate protection keys in the expected order\n");   \
          exit(-1);                                                            \
        }                                                                      \
      }                                                                        \
      *n_to_alloc = 0;                                                         \
    }                                                                          \
  }                                                                            \
  char *ia2_stackptrs[n + 1] IA2_SHARED_DATA;                                  \
  __attribute__((constructor)) static void init_stacks() {                     \
    ensure_pkeys_allocated(&ia2_n_pkeys_to_alloc);                             \
    for (int i = 0; i < n + 1; i++) {                                          \
      char *stack_start = allocate_stack(i);                                   \
      protect_stack(i, stack_start);                                           \
      /* Each stack frame start + 8 is initially 16-byte aligned. */           \
      ia2_stackptrs[i] = stack_start + STACK_SIZE - 8;                         \
    }                                                                          \
  }
