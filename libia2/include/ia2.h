#pragma once

#include <assert.h>
#include <errno.h>
#include <link.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#ifdef LIBIA2_INSECURE
#define INIT_COMPARTMENT(n)
#define IA2_WRPKRU ""
#define IA2_RDPKRU ""
#else
#define INIT_COMPARTMENT(n) _INIT_COMPARTMENT(n)
#define IA2_WRPKRU "wrpkru"
#define IA2_RDPKRU "rdpkru"
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

// Obtain a string corresponding to errno in a threadsafe fashion.
#define errno_s (strerror_l(errno, uselocale((locale_t)0)))

// TODO: Find a better way to compute these offsets for ia2_untrusted_stackptr.
#define STACK(n) _STACK(n)
#define _STACK(n) STACK_##n
#define STACK_0 "0"
#define STACK_1 "8"
#define STACK_2 "16"
#define STACK_3 "24"
#define STACK_4 "32"
#define STACK_5 "40"
#define STACK_6 "48"
#define STACK_7 "56"
#define STACK_8 "64"
#define STACK_9 "72"
#define STACK_10 "80"
#define STACK_11 "88"
#define STACK_12 "96"
#define STACK_13 "104"
#define STACK_14 "112"
#define STACK_15 "120"

#define STACK_SIZE (4 * 1024 * 1024)

#define PAGE_SIZE 4096

#ifdef LIBIA2_INSECURE
#define pkey_mprotect insecure_pkey_mprotect
static int insecure_pkey_mprotect(void *ptr, size_t len, int prot, int pkey) {
  return 0;
}
#define INIT_RUNTIME(n)                                                        \
  int ia2_n_pkeys_to_alloc = 0;                                                \
  INIT_RUNTIME_COMMON(n)
#else
#define INIT_RUNTIME(n)                                                        \
  int ia2_n_pkeys_to_alloc = n;                                                \
  INIT_RUNTIME_COMMON(n)
#endif

#define INIT_RUNTIME_COMMON(n)                                                 \
  /* Protect a stack with the ith pkey. */                                     \
  static void protect_stack(int i, char *stack) {                              \
    if (!stack) {                                                              \
      exit(-1);                                                                \
    }                                                                          \
    int res = pkey_mprotect(stack, STACK_SIZE, PROT_READ | PROT_WRITE, i);     \
    if (res == -1) {                                                           \
      printf("Failed to mprotect stack %d (%s)\n", i, errno_s);                \
      exit(-1);                                                                \
    }                                                                          \
  }                                                                            \
                                                                               \
  /* Allocate a fixed-size stack. */                                           \
  static char *allocate_stack(int i) {                                         \
    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,       \
                               MAP_PRIVATE | MAP_ANON, -1, 0);                 \
    if (stack == MAP_FAILED) {                                                 \
      printf("Failed to allocate stack %d (%s)\n", i, errno_s);                \
      exit(-1);                                                                \
    }                                                                          \
    return stack;                                                              \
  }                                                                            \
  /* Ensure that all required pkeys are allocated. */                          \
  void ensure_pkeys_allocated(int *n_to_alloc) {                               \
    if (*n_to_alloc != 0) {                                                    \
      for (int pkey = 1; pkey <= *n_to_alloc; pkey++) {                        \
        int allocated = pkey_alloc(0, 0);                                      \
        if (allocated < 0) {                                                   \
          printf("Failed to allocate protection key %d (%s)\n", pkey,          \
                 errno_s);                                                     \
          exit(-1);                                                            \
        }                                                                      \
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
