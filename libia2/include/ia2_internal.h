/* This file should only be included by ia2.h and contains internal
 * implementation details of the IA2 runtime. Any interfaces in this file are
 * subject to change and should not be used directly. */
#pragma once

/* for struct dl_phdr_info */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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

/* Supress unused warning */
#define __IA2_UNUSED __attribute__((__unused__))

/*
 * When IA2_CALL has caller pkey = 0, it just casts the opaque struct to an fn
 * ptr. Otherwise it sets ia2_fn_ptr to the opaque struct's value then calls
 * an indirect call gate depending on the opaque struct's type.
 */
#define __IA2_CALL(opaque, id, pkey)                                           \
  ({                                                                           \
    ia2_fn_ptr = opaque.ptr;                                                   \
    (IA2_TYPE_##id) & __ia2_indirect_callgate_##id##_pkey_##pkey;              \
  })
#define _IA2_CALL(opaque, id, pkey) __IA2_CALL(opaque, id, pkey)

/* clang-format off */
#define REPEATB0(fn, basefn) basefn(0)
#define REPEATB1(fn, basefn) fn(1); REPEATB0(fn, basefn)
#define REPEATB2(fn, basefn) fn(2); REPEATB1(fn, basefn)
#define REPEATB3(fn, basefn) fn(3); REPEATB2(fn, basefn)
#define REPEATB4(fn, basefn) fn(4); REPEATB3(fn, basefn)
#define REPEATB5(fn, basefn) fn(5); REPEATB4(fn, basefn)
#define REPEATB6(fn, basefn) fn(6); REPEATB5(fn, basefn)
#define REPEATB7(fn, basefn) fn(7); REPEATB6(fn, basefn)
#define REPEATB8(fn, basefn) fn(8); REPEATB7(fn, basefn)
#define REPEATB9(fn, basefn) fn(9); REPEATB8(fn, basefn)
#define REPEATB10(fn, basefn) fn(10); REPEATB9(fn, basefn)
#define REPEATB11(fn, basefn) fn(11); REPEATB10(fn, basefn)
#define REPEATB12(fn, basefn) fn(12); REPEATB11(fn, basefn)
#define REPEATB13(fn, basefn) fn(13); REPEATB12(fn, basefn)
#define REPEATB14(fn, basefn) fn(14); REPEATB13(fn, basefn)
#define REPEATB15(fn, basefn) fn(15); REPEATB14(fn, basefn)
/* clang-format on */

/* Macro to repeatedly apply a function or function-like macro `fn` a given
number of times, passing the index to each invocation. The passed index `n` is
first, followed by n-1 and so on. For the base case of 0, `basefn` is applied
instead of `fn`. */
#define REPEATB(n, fn, basefn) REPEATB##n(fn, basefn)
/* Handy as the base-case for repeating from N to 1, excluding 1. */
#define nop_macro(x)

// TODO: Do we want to use sysconf(3) here?
#define PAGE_SIZE 4096

#define STACK_SIZE (4 * 1024 * 1024)

/* clang-format can't handle inline asm in macros */
/* clang-format off */
#define _IA2_DEFINE_SIGNAL_HANDLER(function, pkey)    \
    __asm__(".global ia2_sighandler_" #function "\n"  \
            "ia2_sighandler_" #function ":\n"         \
            "movq %rcx, %r10\n"                       \
            "movq %rdx, %r11\n"                       \
            "movq %rax, %r12\n"                       \
            "xorl %ecx, %ecx\n"                       \
            "xorl %edx, %edx\n"                       \
            "mov_pkru_eax " #pkey "\n"                \
            "wrpkru\n"                                \
            "movq %r12, %rax\n"                       \
            "movq %r11, %rdx\n"                       \
            "movq %r10, %rcx\n"                       \
            "jmp " #function "\n")
/* clang-format on */

/// Protect pages in the given shared object
///
/// \param info dynamic linker information for the current object
/// \param size size of \p info in bytes
/// \param data pointer to a PhdrSearchArgs structure
///
/// The callback passed to dl_iterate_phdr in the constructor inserted by
/// ia2_compartment_init.inc to pkey_mprotect the pages corresponding to the
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

// Obtain a string corresponding to errno in a threadsafe fashion.
#define errno_s (strerror_l(errno, uselocale((locale_t)0)))

#define PTRS_PER_PAGE (PAGE_SIZE / sizeof(void *))
#define IA2_MAX_THREADS (PTRS_PER_PAGE)

/* clang-format can't handle inline asm in macros */
/* clang-format off */
/* Allocate and protect the stack for this thread's i'th compartment. */
#define ALLOCATE_COMPARTMENT_STACK_AND_SETUP_TLS(i)                            \
  {                                                                            \
    __IA2_UNUSED extern __thread void *ia2_stackptr_##i;                       \
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
        "rdpkru\n"                                                             \
        "# save pkru in r12d. XXX: if a callee here spills r12, it could\n"    \
        "# be corrupted by another thread subsequently corrupt the pkru\n"     \
        "# when we restore it from r12d\n"                                     \
        "mov %%eax,%%r12d\n"                                                   \
        "# write new pkru\n"                                                   \
        "mov_pkru_eax " #i "\n"                                                \
        "wrpkru\n"                                                             \
        "# save current rsp onto compartment stack\n"                          \
        "mov %%rsp,(%%r10)\n"                                                  \
        "# switch onto compartment stack\n"                                    \
        "mov %%r10,%%rsp\n"                                                    \
        "# align stack\n"                                                      \
        "sub $0x8,%%rsp\n"                                                     \
        "# run init_tls_i on the compartment's stack\n"                        \
        "call init_tls_" #i "\n"                                               \
        "# undo stack align\n"                                                 \
        "add $0x8,%%rsp\n"                                                     \
        "# switch to old stack and restore recently-alloc'd stack to r10\n"    \
        "mov %%rsp,%%r10\n"                                                    \
        "mov (%%r10),%%rsp\n"                                                  \
        "mov ia2_stackptr_" #i "@GOTTPOFF(%%rip),%%r11\n"                      \
        "# check that stack pointer holds NULL\n"                              \
        "cmpq $0x0,%%fs:(%%r11)\n"                                             \
        "je .Lfresh_init" #i "\n"                                              \
        "mov $" #i ",%%rdi\n"                                                  \
        "call ia2_reinit_stack_err\n"                                          \
        "ud2\n"                                                                \
        ".Lfresh_init" #i ":\n"                                                \
        "# store the stack addr in the stack pointer\n"                        \
        "mov %%r10,%%fs:(%%r11)\n"                                             \
        "# restore old pkru\n"                                                 \
        "mov %%r12d,%%eax\n"                                                   \
        "# zero ecx+edx as precondition of wrpkru\n"                           \
        "xor %%ecx,%%ecx\n"                                                    \
        "xor %%edx,%%edx\n"                                                    \
        "wrpkru\n"                                                             \
        :                                                                      \
        : "rax"(stack)                                                         \
        : "rdi", "rcx", "rdx", "r10", "r11", "r12");                           \
  }
/* clang-format on */

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

#define declare_init_tls_fn(n) void init_tls_##n(void);

#define _IA2_INIT_RUNTIME(n)                                                   \
  int ia2_n_pkeys_to_alloc = n;                                                \
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
  /* runtime. */                                                               \
  __thread void *ia2_stackptr_0 __attribute__((aligned(4096)));                \
  /* Include one page of padding after ia2_stackptr_0 to ensure that the */    \
  /* last page of the TLS segment of compartment 0 does not contain any */     \
  /* variables that will be used, because the last page-1 bytes may be */      \
  /* pkey_mprotected by the next compartment depending on sizes/alignment. */  \
  __thread char ia2_threadlocal_padding[PAGE_SIZE] __attribute__((used));      \
                                                                               \
  REPEATB(n, declare_init_tls_fn, nop_macro);                                  \
  /* Confirm that stack pointers for compartments 0 and 1 are at least 4K */   \
  /* apart. */                                                                 \
  void verify_tls_padding(void) {                                              \
    /* It's safe to depend on ia2_stackptr_1 existing because all users of */  \
    /* IA2 will have at least one compartment other than the untrusted one. */ \
    extern __thread void *ia2_stackptr_1;                                      \
    if (labs((intptr_t)&ia2_stackptr_1 - (intptr_t)&ia2_stackptr_0) <          \
        0x1000) {                                                              \
      printf("ia2_stackptr_1 is too close to ia2_stackptr_0\n");               \
      exit(1);                                                                 \
    }                                                                          \
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
    REPEATB(n, return_stackptr_if_compartment,                                 \
            return_stackptr_if_compartment);                                   \
    return NULL;                                                               \
  }                                                                            \
                                                                               \
  __attribute__((weak)) void init_stacks_and_setup_tls(void) {                 \
    verify_tls_padding();                                                      \
    REPEATB(n, ALLOCATE_COMPARTMENT_STACK_AND_SETUP_TLS, nop_macro);           \
    /* allocate an unprotected stack for the untrusted compartment */          \
    ia2_stackptr_0 = allocate_stack(0);                                        \
  }                                                                            \
                                                                               \
  __attribute__((constructor)) static void ia2_init(void) {                    \
    /* Set up global resources. */                                             \
    ensure_pkeys_allocated(&ia2_n_pkeys_to_alloc);                             \
    /* Initialize stacks for the main thread/ */                               \
    init_stacks_and_setup_tls();                                               \
  }