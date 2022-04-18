#pragma once
#include <errno.h>
#include <link.h>
#include <stdio.h>
#include <sys/mman.h>

#include "pkey_init.h"
#include "scrub_registers.h"

// Attribute for variables that can be accessed from any untrusted compartments.
#define IA2_SHARED_DATA __attribute__((section("ia2_shared_data")))

#define IA2_WRAP_FUNCTION(name) __asm__(".symver " #name ",__ia2_" #name "@IA2")

#define XSTR(s) STR(s)
#define STR(s) #s
// TODO: Incorporate __FILE_NAME__ and __COUNTER__ if possible to make this more
// flexible.
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

#define IA2_WRAPPER(target, ty, caller_pkey, target_pkey)                      \
  __asm__(IA2_WRAPPER_##ty(target, caller_pkey, target_pkey));                 \
  extern struct IA2_fnptr_##ty##_inner_t __ia2_##target;

// Takes a function pointer `target` and returns an opaque pointer for its call
// gate wrapper.
#define IA2_FNPTR_WRAPPER(target, ty, caller_pkey, target_pkey)                \
  ({                                                                           \
    static struct IA2_fnptr_##ty##_inner_t *target_ptr __asm__(                \
        UNIQUE_STR(#target)) __attribute__((used));                            \
    static void *wrapper __asm__("__ia2_" UNIQUE_STR(#target));                \
    target_ptr = (struct IA2_fnptr_##ty##_inner_t *)target;                    \
    __asm__(IA2_FNPTR_WRAPPER_##ty(target, caller_pkey, target_pkey));         \
    (struct IA2_fnptr_##ty){(struct IA2_fnptr_##ty##_inner_t *)&wrapper};      \
  })

// Takes an opaque pointer `target` and returns a function pointer for its call
// gate wrapper.
#define IA2_FNPTR_UNWRAPPER(target, ty, caller_pkey, target_pkey)              \
  ({                                                                           \
    static struct IA2_fnptr_##ty##_inner_t *target_ptr __asm__(                \
        UNIQUE_STR(#target)) __attribute__((used));                            \
    static void *wrapper __asm__("__ia2_" UNIQUE_STR(#target));                \
    target_ptr = target.ptr;                                                   \
    __asm__(IA2_FNPTR_UNWRAPPER_##ty(target, caller_pkey, target_pkey));       \
    (IA2_FNPTR_TYPE_##ty) & wrapper;                                           \
  })

// Takes a mangled type name and returns a NULL opaque pointer
#define IA2_NULL_FNPTR(ty)                                                     \
  (struct IA2_fnptr_##ty) { (struct IA2_fnptr_##ty##_inner_t *)NULL }

// Checks if an opaque pointer is null
#define IA2_FNPTR_IS_NULL(target) (target.ptr == NULL)

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
