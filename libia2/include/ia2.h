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

#define INIT_COMPARTMENT_COMMON(n)                                             \
  __thread void *ia2_stackptr_##n __attribute__((used));

#ifdef LIBIA2_INSECURE

#define INIT_COMPARTMENT(n) INIT_COMPARTMENT_COMMON(n)

#if !defined(IA2_WRPKRU)
#define IA2_WRPKRU ""
#endif
#define IA2_RDPKRU ""

#else

#define INIT_COMPARTMENT(n)                                                    \
  INIT_COMPARTMENT_COMMON(n)                                                   \
  _INIT_COMPARTMENT(n)

#if !defined(IA2_WRPKRU)
#define IA2_WRPKRU "wrpkru"
#endif
#define IA2_RDPKRU "rdpkru"

#endif

#define IA2_DEFINE_SIGHANDLER(function)               \
    void ia2_sighandler_##function(int);              \
    __asm__(".global ia2_sighandler_" #function "\n"  \
            "ia2_sighandler_" #function ":\n"         \
            "movq %rcx, %r10\n"                       \
            "movq %rdx, %r11\n"                       \
            "xorl %ecx, %ecx\n"                       \
            "xorl %edx, %edx\n"                       \
            "xorl %eax, %eax\n"                       \
            "wrpkru\n"                                \
            "movq %r11, %rdx\n"                       \
            "movq %r10, %rcx\n"                       \
            "jmp " #function "\n")

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

// Attribute for read-write variables that can be accessed from any untrusted
// compartments.
#define IA2_SHARED_DATA __attribute__((section("ia2_shared_data")))

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

/* clang-format can't handle inline asm in macros */
/* clang-format off */
/* Allocate and protect the stack for this thread's i'th compartment. */
#define ALLOCATE_COMPARTMENT_STACK(i)                                          \
  {                                                                            \
    extern __thread void *ia2_stackptr_##i;                                    \
                                                                               \
    register void *stack asm("rax") = allocate_stack(i);                       \
                                                                               \
    /* We must change the pkru to write the stack pointer because each */      \
    /* stack pointer is part of the compartment whose stack it points to. */   \
    __asm__ volatile(                                                          \
        "mov %0, %%r10\n"                                                      \
        "# zero ecx as precondition of rdpkru\n"                               \
        "xor %%ecx,%%ecx\n"                                                    \
        "# eax = old pkru; also zeroes edx, which is required for wrpkru\n"    \
        IA2_RDPKRU "\n"                                                        \
        "# save pkru in r12d\n"                                                \
        "mov %%eax,%%r12d\n"                                                   \
        "# write new pkru\n"                                                   \
        "mov_pkru_eax " #i "\n"                                                \
        IA2_WRPKRU "\n"                                                        \
        "mov ia2_stackptr_" #i "@GOTTPOFF(%%rip), %%r11\n"                     \
        "# check that stack pointer holds NULL\n"                              \
        "cmpq $0x0,%%fs:(%%r11)\n"                                             \
        "je .Lfresh_init" #i "\n"                                              \
        "mov $" #i ", %%rdi\n"                                                 \
        "call ia2_reinit_stack_err\n"                                          \
        "ud2\n"                                                                \
        ".Lfresh_init" #i ":\n"                                                \
        "# store the stack addr in the stack pointer\n"                        \
        "mov %%r10, %%fs:(%%r11)\n"                                            \
        "# restore old pkru\n"                                                 \
        "mov %%r12d,%%eax\n"                                                   \
        IA2_WRPKRU "\n"                                                        \
        :                                                                      \
        : "rax"(stack)                                                         \
        : "rdi", "rcx", "rdx", "r10", "r11", "r12");                           \
  }
/* clang-format on */

#define PKRU(pkey) (~((3 << (2 * pkey)) | 3))

#define return_stackptr_if_compartment(compartment)                            \
  if (pkru == PKRU(compartment)) {                                             \
    register void *out asm("rax");                                             \
    __asm__ volatile(                                                          \
        "mov %%fs:(0), %%rax\n"                                                \
        "addq ia2_stackptr_" #compartment "@GOTTPOFF(%%rip), %%rax\n"          \
        : "=a"(out)                                                            \
        :                                                                      \
        :);                                                                    \
    return out;                                                                \
  }

#define INIT_RUNTIME_COMMON(n)                                                 \
  /* Allocate a fixed-size stack and protect it with the ith pkey. */          \
  /* Returns the top of the stack, not the base address of the allocation. */  \
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
    /* Each stack frame start + 8 is initially 16-byte aligned. */             \
    return stack + STACK_SIZE - 8;                                             \
  }                                                                            \
  /* The 0th compartment is unprivileged and does not protect its memory, */   \
  /* so declare its stack pointer in the shared object that sets up the */     \
  /* runtime instead of using the INIT_COMPARTMENT macro for it. */            \
  __thread void *ia2_stackptr_0 __attribute__((aligned(4096)));                \
  /* Include one page of padding after ia2_stackptr_0 to ensure that the */    \
  /* last page of the TLS segment of compartment 0 does not contain any */     \
  /* variables that will be used, because the last page-1 bytes may be */      \
  /* pkey_mprotected by the next compartment depending on sizes/alignment. */  \
  __thread char ia2_threadlocal_padding[PAGE_SIZE] __attribute__((used));      \
                                                                               \
  /* Static dispatch hack: */                                                  \
  /* We declare an init_tls_N function for each possible compartment, but */   \
  /* each compartment's shared library is responsible for providing the  */    \
  /* definition of its respective function. As such, only those */             \
  /* corresponding to actual compartments in the program will have a*/         \
  /* definition at load-time. We skip the ones that don't exist with */        \
  /* statically-determined control flow (goto based on macro arg), so the */   \
  /* compiler will remove references to the ones that don't exist and give */  \
  /* us clean, straight-line code calling only the functions for */            \
  /* compartments that do exist. */                                            \
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
    /* Confirm that stack pointers for compartments 0 and 1 are at least 4K */ \
    /* apart. */                                                               \
    /* It's safe to depend on ia2_stackptr_1 existing because all users of */  \
    /* IA2 will have at least one compartment other than the untrusted one. */ \
    extern __thread void *ia2_stackptr_1;                                      \
    if (labs((intptr_t)&ia2_stackptr_1 - (intptr_t)&ia2_stackptr_0) <          \
        0x1000) {                                                              \
      printf("ia2_stackptr_1 is too close to ia2_stackptr_0\n");               \
      exit(1);                                                                 \
    }                                                                          \
    goto compartment##n;                                                       \
  compartment15:                                                               \
    protect_tls_for_compartment(15);                                           \
  compartment14:                                                               \
    protect_tls_for_compartment(14);                                           \
  compartment13:                                                               \
    protect_tls_for_compartment(13);                                           \
  compartment12:                                                               \
    protect_tls_for_compartment(12);                                           \
  compartment11:                                                               \
    protect_tls_for_compartment(11);                                           \
  compartment10:                                                               \
    protect_tls_for_compartment(10);                                           \
  compartment9:                                                                \
    protect_tls_for_compartment(9);                                            \
  compartment8:                                                                \
    protect_tls_for_compartment(8);                                            \
  compartment7:                                                                \
    protect_tls_for_compartment(7);                                            \
  compartment6:                                                                \
    protect_tls_for_compartment(6);                                            \
  compartment5:                                                                \
    protect_tls_for_compartment(5);                                            \
  compartment4:                                                                \
    protect_tls_for_compartment(4);                                            \
  compartment3:                                                                \
    protect_tls_for_compartment(3);                                            \
  compartment2:                                                                \
    protect_tls_for_compartment(2);                                            \
  compartment1:                                                                \
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
  /* Forbid overwriting an existing stack. */                                  \
  _Noreturn void ia2_reinit_stack_err(int i) {                                 \
    printf("compartment %d in thread %d tried to allocate existing stack\n",   \
           i, gettid());                                                       \
    exit(1);                                                                   \
  }                                                                            \
                                                                               \
  /* Returns `&ia2_stackptr_N` given a pkru value for the Nth compartment. */  \
  void **ia2_stackptr_for_pkru(uint32_t pkru) {                                \
    goto compartment##n;                                                       \
  compartment15:                                                               \
    return_stackptr_if_compartment(15);                                        \
  compartment14:                                                               \
    return_stackptr_if_compartment(14);                                        \
  compartment13:                                                               \
    return_stackptr_if_compartment(13);                                        \
  compartment12:                                                               \
    return_stackptr_if_compartment(12);                                        \
  compartment11:                                                               \
    return_stackptr_if_compartment(11);                                        \
  compartment10:                                                               \
    return_stackptr_if_compartment(10);                                        \
  compartment9:                                                                \
    return_stackptr_if_compartment(9);                                         \
  compartment8:                                                                \
    return_stackptr_if_compartment(8);                                         \
  compartment7:                                                                \
    return_stackptr_if_compartment(7);                                         \
  compartment6:                                                                \
    return_stackptr_if_compartment(6);                                         \
  compartment5:                                                                \
    return_stackptr_if_compartment(5);                                         \
  compartment4:                                                                \
    return_stackptr_if_compartment(4);                                         \
  compartment3:                                                                \
    return_stackptr_if_compartment(3);                                         \
  compartment2:                                                                \
    return_stackptr_if_compartment(2);                                         \
  compartment1:                                                                \
    return_stackptr_if_compartment(1);                                         \
  compartment0:                                                                \
    return_stackptr_if_compartment(0);                                         \
    return NULL;                                                               \
  }                                                                            \
                                                                               \
  __attribute__((weak)) void init_stacks(void) {                               \
    goto compartment##n;                                                       \
  compartment15:                                                               \
    ALLOCATE_COMPARTMENT_STACK(15)                                             \
  compartment14:                                                               \
    ALLOCATE_COMPARTMENT_STACK(14)                                             \
  compartment13:                                                               \
    ALLOCATE_COMPARTMENT_STACK(13)                                             \
  compartment12:                                                               \
    ALLOCATE_COMPARTMENT_STACK(12)                                             \
  compartment11:                                                               \
    ALLOCATE_COMPARTMENT_STACK(11)                                             \
  compartment10:                                                               \
    ALLOCATE_COMPARTMENT_STACK(10)                                             \
  compartment9:                                                                \
    ALLOCATE_COMPARTMENT_STACK(9)                                              \
  compartment8:                                                                \
    ALLOCATE_COMPARTMENT_STACK(8)                                              \
  compartment7:                                                                \
    ALLOCATE_COMPARTMENT_STACK(7)                                              \
  compartment6:                                                                \
    ALLOCATE_COMPARTMENT_STACK(6)                                              \
  compartment5:                                                                \
    ALLOCATE_COMPARTMENT_STACK(5)                                              \
  compartment4:                                                                \
    ALLOCATE_COMPARTMENT_STACK(4)                                              \
  compartment3:                                                                \
    ALLOCATE_COMPARTMENT_STACK(3)                                              \
  compartment2:                                                                \
    ALLOCATE_COMPARTMENT_STACK(2)                                              \
  compartment1:                                                                \
    ALLOCATE_COMPARTMENT_STACK(1)                                              \
  compartment0:                                                                \
    /* allocate an unprotected stack for the untrusted compartment */          \
    ia2_stackptr_0 = allocate_stack(0);                                        \
  }                                                                            \
                                                                               \
  __attribute__((constructor)) static void ia2_init(void) {                    \
    /* Set up global resources. */                                             \
    ensure_pkeys_allocated(&ia2_n_pkeys_to_alloc);                             \
    /* Initialize stacks for the main thread/ */                               \
    protect_tls();                                                             \
    init_stacks();                                                             \
  }
