#pragma once
#include <errno.h>
#include <link.h>
#include <stdio.h>
#include <sys/mman.h>

#include "pkey_init.h"
#include "scrub_registers.h"

#define IA2_WRAP_FUNCTION(name) __asm__(".symver " #name ",__ia2_" #name "@IA2")

#define XSTR(s) STR(s)
#define STR(s) #s
#define UNIQUE_STR(s) s "_line_" XSTR(__LINE__)

#ifdef LIBIA2_INSECURE
#define INIT_COMPARTMENT(n) DECLARE_PADDING_SECTIONS
#define IA2_WRPKRU
#else
#define INIT_COMPARTMENT(n) _INIT_COMPARTMENT(n)
#define IA2_WRPKRU "wrpkru"
#endif

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

// Attribute for read-only variables that can be accessed from any untrusted
// compartments.
#define IA2_SHARED_RODATA __attribute__((section("ia2_shared_rodata")))

#define _IA2_DEFINE_SHARED_STR(name, s)                                        \
  __asm__(".section ia2_shared_rodata\n" \
            ".equ str" XSTR(name) XSTR(__LINE__) ", .\n"\
            ".asciz \"" s "\"\n" \
            ".previous")

// Defines a string literal which may be read from any compartment.
//
// This macro may be used in the global scope or in a function. Use `&name` to
// access the `char *` created by this macro. This is equivalent to defining
// `char name = s` in the global scope so `name` must be a unique identifier.
#define IA2_SHARED_STR(name, s)                                                \
  extern char name __asm__("str" XSTR(name) XSTR(__LINE__));                   \
  const size_t __ia2_sizeof_##name = sizeof(s);                                \
  _IA2_DEFINE_SHARED_STR(name, s);

// Defines a string literal which may be read from any compartment and expands
// to a `char *` pointing to the string literal.
//
// This macro may only be used in functions.
#define IA2_SHARED_STR_FN_SCOPE(s)                                             \
  ({                                                                           \
    /* The following identifier doesn't matter since the assembler only sees   \
     * the name in its asm label which should be unique. Since the string      \
     * literal may contain characters which the assembler doesn't accept in    \
     * symbol names (e.g. ',', '>', '[') we can't use `s` to generate this     \
     * unique name and instead resort to just appending "str" to the line      \
     * number. */                                                              \
    static char str __asm__("str" XSTR(__LINE__));                             \
    _IA2_DEFINE_SHARED_STR(, s);                                               \
    &str;                                                                      \
  })

// Declares a wrapper for the function `target`.
//
// The wrapper expects caller pkey 0 and uses the given target_pkey. This macro
// may be used both in a function and in the global scope. Use
// IA2_WRAPPER(target) or IA2_WRAPPER_FN_SCOPE(target) to get the wrapper as an
// opaque pointer type.
#define IA2_DECLARE_WRAPPER(target, ty, target_pkey)                           \
  /* Forward declare the function and mark it as used */                       \
  /* TODO: This needed `static` to allow using it within a function */ \
  __attribute__((used)) static void *target##_unused = (void *)target;                                 \
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

// Declare a wrapper for the function `target` and expands to an opaque pointer
// expression for the wrapper.
//
// This macro may only be used inside functions.
#define IA2_DECLARE_WRAPPER_FN_SCOPE(target, ty, target_pkey)                  \
  ({                                                                           \
    IA2_DECLARE_WRAPPER(target, ty, target_pkey);                              \
    IA2_WRAPPER_FN_SCOPE(target, target_pkey);                                 \
  })

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
    static struct IA2_fnptr_##ty##_inner_t *target_ptr __asm__(                \
        UNIQUE_STR(#ty)) __attribute__((used));                                \
    static void *wrapper __asm__("__ia2_" UNIQUE_STR(#ty));                    \
    target_ptr = target.ptr;                                                   \
    __asm__(IA2_CALL_##ty(target, ty, caller_pkey, 0));                        \
    (IA2_FNPTR_TYPE_##ty) & wrapper;                                           \
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

#define IA2_WRAPPER_ADDR(target) (void *)target.ptr

// lhs: opaque pointer, rhs: void *
// This is safe because we must use IA2_CALL before calling the pointer
#define IA2_ASSIGN_FN_SCOPE(lhs, rhs)                                          \
  lhs = (typeof(lhs)) { rhs }

// We must declare the sections used to pad the end of each program header
// segment to make sure their rwx permissions match the segment they're placed
// in. Otherwise if the padding sections are declared in the linker script
// without any input sections they and their corresponding segment will default
// to rwx. We avoid using .balign to align the sections at the start of each
// segment because it inserts a fill value (defaults to 0) which may break some
// sections (e.g.  insert null pointers into .init_array).
#define NEW_SECTION(name)                                                      \
  __asm__(".section " #name                                                    \
          "\n"                                                                 \
          ".previous");

#define DECLARE_PADDING_SECTIONS                                               \
  NEW_SECTION(".fini_padding");                                                \
  NEW_SECTION(".rela.plt_padding");                                            \
  NEW_SECTION(".eh_frame_padding");                                            \
  NEW_SECTION(".dynamic_padding");                                             \
  NEW_SECTION(".gnu_hash_padding");                                            \
  NEW_SECTION(".dynsym_padding");                                              \
  NEW_SECTION(".dynstr_padding");                                              \
  NEW_SECTION(".version_padding");                                             \
  NEW_SECTION(".bss_padding");

// Initializes a compartment with protection key `n` when the ELF invoking this
// macro is loaded. This must only be called once for each key. The compartment
// includes all segments in the ELF except the `ia2_shared_data` section, if one
// exists.
#define _INIT_COMPARTMENT(n)                                                   \
  DECLARE_PADDING_SECTIONS;                                                    \
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
