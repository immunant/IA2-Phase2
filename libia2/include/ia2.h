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

#include "scrub_registers.h"

#define XSTR(s) STR(s)
#define STR(s) #s
#define PASTE3_(x, y, z) x##y##z
#define PASTE3(x, y, z) PASTE3_(x, y, z)

#define PASTE4_(w, x, y, z) w##x##y##z
#define PASTE4(w, x, y, z) PASTE4_(w, x, y, z)

#ifdef LIBIA2_INSECURE
#define INIT_COMPARTMENT(n)
#define IA2_WRPKRU
#else
#define INIT_COMPARTMENT(n) _INIT_COMPARTMENT(n)
#define IA2_WRPKRU "wrpkru"
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

// This emits the 5 bytes correponding to the movl $PKRU, %eax instruction
asm(".macro mov_pkru_eax pkey\n"
    ".byte 0xb8\n"
    ".long ~((3 << (2 * \\pkey)) | 3)\n"
    ".endm");

// This emits the 5 bytes correponding to the movl $PKRU, %eax instruction
asm(".macro mov_mixed_pkru_eax pkey0, pkey1\n"
    ".byte 0xb8\n"
    ".long ~((3 << (2 * \\pkey0)) | (3 << (2 * \\pkey1)) | 3)\n"
    ".endm");

// Attribute for read-write variables that can be accessed from any untrusted
// compartments.
#define IA2_SHARED_DATA __attribute__((section("ia2_shared_data")))

// Declares a wrapper for the function `target`.
//
// The wrapper expects caller pkey 0 and uses the given target_pkey. This macro
// may be used both in a function and in the global scope. Use
// IA2_WRAPPER(target) or IA2_WRAPPER_FN_SCOPE(target) to get the wrapper as an
// opaque pointer type.
#define IA2_DECLARE_WRAPPER(target, ty, target_pkey)                           \
  /* We should redeclare the target function with the used attribute. However, \
   * this doesn't work in clang, so instead we define a pointer and initialize \
   * it to the targetfunction. This pointer is unused, but we mark it as used  \
   * to allow compiling with Wno-unused-variable. */                           \
  __attribute__((used)) static void *target##_ptr = target;                    \
  /* Create an identifier to get the wrapper's address with                    \
   * IA2_WRAPPER/IA2_WRAPPER_FN_SCOPE */                                       \
  extern struct IA2_fnptr_##ty##_inner_t __ia2_##target##_0_##target_pkey;     \
  /* Create an identifier to get the wrapper's type with                       \
   * IA2_WRAPPER/IA2_WRAPPER_FN_SCOPE */                                       \
  extern struct IA2_fnptr_##ty __ia2_##target##_wrapper;

// Defines a wrapper for the function `target`.
//
// The wrapper expects caller pkey 0 and uses the given target_pkey. This macro
// may be used both in a function and in the global scope. Use
// IA2_WRAPPER(target) or IA2_WRAPPER_FN_SCOPE(target) to get the wrapper as an
// opaque pointer type.
#define IA2_DEFINE_WRAPPER(target, ty, target_pkey)                            \
  /* Define the wrapper in asm */                                              \
  __asm__(IA2_DEFINE_WRAPPER_##ty(target, 0, target_pkey));                    \
  IA2_DECLARE_WRAPPER(target, ty, target_pkey)

// Expands to an opaque pointer expression for a wrapper defined by
// IA2_DEFINE_WRAPPER.
//
// This macro may only be used in the global scope.
#define IA2_WRAPPER(target, target_pkey)                                       \
  { &__ia2_##target##_0_##target_pkey }

// Expands to an opaque pointer expression for a wrapper defined by
// IA2_DEFINE_WRAPPER.
//
// This macro may only be used inside functions.
#define IA2_WRAPPER_FN_SCOPE(target, target_pkey)                              \
  (typeof(__ia2_##target##_wrapper)) { &__ia2_##target##_0_##target_pkey }

// Defines a wrapper for the function `target` and expands to an opaque pointer
// expression for the wrapper.
//
// This macro may only be used inside functions.
#define IA2_DEFINE_WRAPPER_FN_SCOPE(target, ty, target_pkey)                   \
  ({                                                                           \
    IA2_DEFINE_WRAPPER(target, ty, target_pkey);                               \
    IA2_WRAPPER_FN_SCOPE(target, target_pkey);                                 \
  })

// Defines a wrapper for the opaque pointer `target` and expands to a function
// pointer expression for this wrapper.
//
// The wrapper expects the given caller_pkey and uses target pkey 0. This macro
// may only be used inside functions. The resulting function pointer expression
// may be assigned an identifier or called immediately. Since the rewritten
// headers replace function pointer types with opaque pointer types, calling it
// immediately is the more ergonomic approach.
#define IA2_CALL(target, ty, caller_pkey)                                      \
  ({                                                                           \
    /* Declare the wrapper for the function pointer. This will be defined in   \
     * the following asm statement. */                                         \
    extern struct IA2_fnptr_##ty *PASTE4(__ia2_, ty, _line_, __LINE__);        \
    /* Since the function pointer we're calling may be on the stack, it        \
     * might not have a fixed address. Instead of calling that pointer, we     \
     * instead define a new pointer with a fixed address and initialize it     \
     * with the first pointer. The new pointer's symbol isn't visible to ld,   \
     * so its identifier (`target_ptr`) doesn't matter. We need to make it     \
     * visible to the assembler though which requires the asm label. The       \
     * assembler sees the name inside the asm label rather than the pointer's  \
     * identifier. */                                                          \
    __attribute__((used)) static void *target_ptr __asm__(                     \
        XSTR(PASTE4(__ia2_, ty, _target_ptr_line_, __LINE__)));                \
    /* Set the new function pointer to the target address */                   \
    target_ptr = (void *)target.ptr;                                           \
    /* Define the wrapper for the target function pointer */                   \
    __asm__(IA2_CALL_##ty(target, ty, caller_pkey, 0));                        \
    /* Cast the address of the wrapper declared above to the function pointer  \
     * type and return it */                                                   \
    (IA2_FNPTR_TYPE_##ty)(&PASTE4(__ia2_, ty, _line_, __LINE__));              \
  })

// Expands to a NULL pointer expression which can be coerced to any opaque
// pointer type.
//
// This macro may only be used in the global scope.
#define IA2_NULL_FNPTR                                                         \
  { (void *)NULL }

// Sets the given opaque pointer to NULL.
#define IA2_NULL_FNPTR_FN_SCOPE(ptr)                                           \
  ptr = (typeof(ptr)) { (void *)NULL }

// Checks if an opaque pointer is null
#define IA2_FNPTR_IS_NULL(target) (target.ptr == NULL)

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
      printf("Failed to mprotect stack %d (%s)\n", i, errno_s);                \
      exit(-1);                                                                \
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
