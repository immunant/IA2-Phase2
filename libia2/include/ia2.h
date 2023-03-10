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
#include <unistd.h>

#include "scrub_registers.h"

#define XSTR(s) STR(s)
#define STR(s) #s
#define PASTE3_(x, y, z) x##y##z
#define PASTE3(x, y, z) PASTE3_(x, y, z)

#define PASTE4_(w, x, y, z) w##x##y##z
#define PASTE4(w, x, y, z) PASTE4_(w, x, y, z)

#define INIT_COMPARTMENT_COMMON(n)                                             \
  __thread void *ia2_stackptr_##n __attribute__((used));

#ifdef LIBIA2_INSECURE
#define INIT_COMPARTMENT(n) INIT_COMPARTMENT_COMMON(n)
#define IA2_WRPKRU ""
#define IA2_RDPKRU ""
#else
#define INIT_COMPARTMENT(n)                                                    \
  INIT_COMPARTMENT_COMMON(n)                                                   \
  _INIT_COMPARTMENT(n)
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
int protect_tls_pages(struct dl_phdr_info *info, size_t size, void *data);

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

#ifdef LIBIA2_INSECURE
#define protect_tls_for_compartment(n)
#else
#define protect_tls_for_compartment(n) init_tls_##n();
#endif

// Initializes a compartment with protection key `n` when the ELF invoking this
// macro is loaded. This must only be called once for each key. The compartment
// includes all segments in the ELF except the `ia2_shared_data` section, if one
// exists.
#define _INIT_COMPARTMENT(n)                                                   \
  extern int ia2_n_pkeys_to_alloc;                                             \
  void ensure_pkeys_allocated(int *n_to_alloc);                                \
  __attribute__((constructor)) static void init_pkey_##n##_ctor() {            \
    ensure_pkeys_allocated(&ia2_n_pkeys_to_alloc);                             \
    struct PhdrSearchArgs args = {                                             \
        .pkey = n,                                                             \
        .address = &init_pkey_##n##_ctor,                                      \
    };                                                                         \
    dl_iterate_phdr(protect_pages, &args);                                     \
  }                                                                            \
  void init_tls_##n(void) {                                                    \
    struct PhdrSearchArgs args = {                                             \
        .pkey = n,                                                             \
        .address = &init_pkey_##n##_ctor,                                      \
    };                                                                         \
    dl_iterate_phdr(protect_tls_pages, &args);                                 \
  }

// Obtain a string corresponding to errno in a threadsafe fashion.
#define errno_s (strerror_l(errno, uselocale((locale_t)0)))

#define STACK_SIZE (4 * 1024 * 1024)

#define PAGE_SIZE 4096
#define PTRS_PER_PAGE (PAGE_SIZE / sizeof(void *))
#define IA2_MAX_THREADS (PTRS_PER_PAGE)

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

/* Allocate and protect the stack for this thread's i'th compartment. */

/* I'm worried about interference from other threads if sensitive */
/* locals in this code (namely old_pkru) get spilled to the */
/* stack. We probably need to write this in assembly. */
#define ALLOCATE_COMPARTMENT_STACK(i)                                          \
  {                                                                            \
    extern __thread void *ia2_stackptr_##i;                                    \
    /*assert(&ia2_stackptr_##i != NULL);*/                                     \
    char *stack = allocate_stack(i);                                           \
                                                                               \
    /* Each stack frame start + 8 is initially 16-byte aligned. */             \
    char *stack_ptr_value = stack + STACK_SIZE - 8;                            \
                                                                               \
    /* We must change the pkru to write the stack pointer because each */      \
    /* stack pointer is part of the compartment whose stack it points to. */   \
                                                                               \
    /* Save the current pkru. */                                               \
    uint32_t old_pkru = 0;                                                     \
    __asm__ volatile(IA2_RDPKRU : "=a"(old_pkru) : "a"(0), "d"(0), "c"(0));    \
    /* Switch to the compartment's pkru to access its stack pointer. */        \
    uint32_t new_pkru = ~((3 << (2 * i)) | 3);                                 \
    __asm__ volatile(IA2_WRPKRU : : "a"(new_pkru), "d"(0), "c"(0));            \
    /* Forbid overwriting an existing stack. */                                \
    if (ia2_stackptr_##i != NULL) {                                            \
      printf("compartment %d in thread %d tried to allocate existing stack\n", \
             i, gettid());                                                     \
      exit(1);                                                                 \
    }                                                                          \
    /* Write the stack pointer. */                                             \
    ia2_stackptr_##i = stack_ptr_value;                                        \
    /* Restore the old pkru. */                                                \
    __asm__(IA2_WRPKRU : : "a"(old_pkru), "d"(0), "c"(0));                     \
  }

#define INIT_RUNTIME_COMMON(n)                                                 \
  /* Allocate a fixed-size stack and protect it with the ith pkey. */          \
  char *allocate_stack(int i) {                                                \
    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,       \
                               MAP_PRIVATE | MAP_ANON, -1, 0);                 \
    if (stack == MAP_FAILED) {                                                 \
      printf("Failed to allocate stack %d (%s)\n", i, errno_s);                \
      exit(-1);                                                                \
    }                                                                          \
    if (i != 0) {                                                              \
      int res = pkey_mprotect(stack, STACK_SIZE, PROT_READ | PROT_WRITE, i);   \
      if (res == -1) {                                                         \
        printf("Failed to mprotect stack %d (%s)\n", i, errno_s);              \
        exit(-1);                                                              \
      }                                                                        \
    }                                                                          \
    return stack;                                                              \
  }                                                                            \
  void init_tls_15(void);                                                      \
  void init_tls_14(void);                                                      \
  void init_tls_13(void);                                                      \
  void init_tls_12(void);                                                      \
  void init_tls_11(void);                                                      \
  void init_tls_10(void);                                                      \
  void init_tls_9(void);                                                       \
  void init_tls_8(void);                                                       \
  void init_tls_7(void);                                                       \
  void init_tls_6(void);                                                       \
  void init_tls_5(void);                                                       \
  void init_tls_4(void);                                                       \
  void init_tls_3(void);                                                       \
  void init_tls_2(void);                                                       \
  void init_tls_1(void);                                                       \
  /* Ensure that TLS is protected in a new thread. */                          \
  void protect_tls(void) {                                                     \
    goto tls_##n;                                                              \
  tls_15:                                                                      \
    protect_tls_for_compartment(15);                                           \
  tls_14:                                                                      \
    protect_tls_for_compartment(14);                                           \
  tls_13:                                                                      \
    protect_tls_for_compartment(13);                                           \
  tls_12:                                                                      \
    protect_tls_for_compartment(12);                                           \
  tls_11:                                                                      \
    protect_tls_for_compartment(11);                                           \
  tls_10:                                                                      \
    protect_tls_for_compartment(10);                                           \
  tls_9:                                                                       \
    protect_tls_for_compartment(9);                                            \
  tls_8:                                                                       \
    protect_tls_for_compartment(8);                                            \
  tls_7:                                                                       \
    protect_tls_for_compartment(7);                                            \
  tls_6:                                                                       \
    protect_tls_for_compartment(6);                                            \
  tls_5:                                                                       \
    protect_tls_for_compartment(5);                                            \
  tls_4:                                                                       \
    protect_tls_for_compartment(4);                                            \
  tls_3:                                                                       \
    protect_tls_for_compartment(3);                                            \
  tls_2:                                                                       \
    protect_tls_for_compartment(2);                                            \
  tls_1:                                                                       \
    protect_tls_for_compartment(1);                                            \
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
                                                                               \
  /* The 0th compartment is unprivileged and does not protect its memory, */   \
  /* so declare its stack pointer in the shared object that sets up the */     \
  /* runtime instead of using the INIT_COMPARTMENT macro for it. */            \
  __thread void *ia2_stackptr_0;                                               \
                                                                               \
  __attribute__((weak)) void init_stacks(void) {                               \
    switch (n) {                                                               \
    case 15:                                                                   \
      ALLOCATE_COMPARTMENT_STACK(15)                                           \
    case 14:                                                                   \
      ALLOCATE_COMPARTMENT_STACK(14)                                           \
    case 13:                                                                   \
      ALLOCATE_COMPARTMENT_STACK(13)                                           \
    case 12:                                                                   \
      ALLOCATE_COMPARTMENT_STACK(12)                                           \
    case 11:                                                                   \
      ALLOCATE_COMPARTMENT_STACK(11)                                           \
    case 10:                                                                   \
      ALLOCATE_COMPARTMENT_STACK(10)                                           \
    case 9:                                                                    \
      ALLOCATE_COMPARTMENT_STACK(9)                                            \
    case 8:                                                                    \
      ALLOCATE_COMPARTMENT_STACK(8)                                            \
    case 7:                                                                    \
      ALLOCATE_COMPARTMENT_STACK(7)                                            \
    case 6:                                                                    \
      ALLOCATE_COMPARTMENT_STACK(6)                                            \
    case 5:                                                                    \
      ALLOCATE_COMPARTMENT_STACK(5)                                            \
    case 4:                                                                    \
      ALLOCATE_COMPARTMENT_STACK(4)                                            \
    case 3:                                                                    \
      ALLOCATE_COMPARTMENT_STACK(3)                                            \
    case 2:                                                                    \
      ALLOCATE_COMPARTMENT_STACK(2)                                            \
    case 1:                                                                    \
      ALLOCATE_COMPARTMENT_STACK(1)                                            \
    case 0:                                                                    \
      /* allocate an unprotected stack for the untrusted compartment */        \
      ia2_stackptr_0 = allocate_stack(0) + STACK_SIZE - 8;                     \
    }                                                                          \
  }                                                                            \
                                                                               \
  __attribute__((constructor)) static void ia2_init(void) {                    \
    /* Set up global resources. */                                             \
    ensure_pkeys_allocated(&ia2_n_pkeys_to_alloc);                             \
    /* Initialize stacks for the main thread/ */                               \
    init_stacks();                                                             \
  }
