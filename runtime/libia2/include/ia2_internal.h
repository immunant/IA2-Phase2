/* This file should only be included by ia2.h and contains internal
 * implementation details of the IA2 runtime. Any interfaces in this file are
 * subject to change and should not be used directly. */
#pragma once

/* struct dl_phdr_info is only defined if _GNU_SOURCE but at rewriter runtime
 * _GNU_SOURCE may not be defined in the user's code prior to the inclusion of
 * <features.h>. As such, forward-declare it here as we only use it opaquely. */
struct dl_phdr_info;

/* for pkey_mprotect */
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

#define IA2_CONCAT_(x, y) x##y
#define IA2_CONCAT(x, y) IA2_CONCAT_(x, y)

/* Fully expand x to a string if x is a macro (e.g. IA2_COMPARTMENT) */
#define IA2_XSTR(x) #x
#define IA2_STR(x) IA2_XSTR(x)

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
#define REPEATB_REV0(fn, basefn, ...) basefn(0, ##__VA_ARGS__)
#define REPEATB_REV1(fn, basefn, ...) REPEATB_REV0(fn, basefn, ##__VA_ARGS__) fn(1, ##__VA_ARGS__)
#define REPEATB_REV2(fn, basefn, ...) REPEATB_REV1(fn, basefn, ##__VA_ARGS__) fn(2, ##__VA_ARGS__)
#define REPEATB_REV3(fn, basefn, ...) REPEATB_REV2(fn, basefn, ##__VA_ARGS__) fn(3, ##__VA_ARGS__)
#define REPEATB_REV4(fn, basefn, ...) REPEATB_REV3(fn, basefn, ##__VA_ARGS__) fn(4, ##__VA_ARGS__)
#define REPEATB_REV5(fn, basefn, ...) REPEATB_REV4(fn, basefn, ##__VA_ARGS__) fn(5, ##__VA_ARGS__)
#define REPEATB_REV6(fn, basefn, ...) REPEATB_REV5(fn, basefn, ##__VA_ARGS__) fn(6, ##__VA_ARGS__)
#define REPEATB_REV7(fn, basefn, ...) REPEATB_REV6(fn, basefn, ##__VA_ARGS__) fn(7, ##__VA_ARGS__)
#define REPEATB_REV8(fn, basefn, ...) REPEATB_REV7(fn, basefn, ##__VA_ARGS__) fn(8, ##__VA_ARGS__)
#define REPEATB_REV9(fn, basefn, ...) REPEATB_REV8(fn, basefn, ##__VA_ARGS__) fn(9, ##__VA_ARGS__)
#define REPEATB_REV10(fn, basefn, ...) REPEATB_REV9(fn, basefn, ##__VA_ARGS__) fn(10, ##__VA_ARGS__)
#define REPEATB_REV11(fn, basefn, ...) REPEATB_REV10(fn, basefn, ##__VA_ARGS__) fn(11, ##__VA_ARGS__)
#define REPEATB_REV12(fn, basefn, ...) REPEATB_REV11(fn, basefn, ##__VA_ARGS__) fn(12, ##__VA_ARGS__)
#define REPEATB_REV13(fn, basefn, ...) REPEATB_REV12(fn, basefn, ##__VA_ARGS__) fn(13, ##__VA_ARGS__)
#define REPEATB_REV14(fn, basefn, ...) REPEATB_REV13(fn, basefn, ##__VA_ARGS__) fn(14, ##__VA_ARGS__)
#define REPEATB_REV15(fn, basefn, ...) REPEATB_REV14(fn, basefn, ##__VA_ARGS__) fn(15, ##__VA_ARGS__)
#define REPEATB0(fn, basefn, ...) basefn(0, ##__VA_ARGS__)
#define REPEATB1(fn, basefn, ...) fn(1, ##__VA_ARGS__) REPEATB0(fn, basefn, ##__VA_ARGS__)
#define REPEATB2(fn, basefn, ...) fn(2, ##__VA_ARGS__) REPEATB1(fn, basefn, ##__VA_ARGS__)
#define REPEATB3(fn, basefn, ...) fn(3, ##__VA_ARGS__) REPEATB2(fn, basefn, ##__VA_ARGS__)
#define REPEATB4(fn, basefn, ...) fn(4, ##__VA_ARGS__) REPEATB3(fn, basefn, ##__VA_ARGS__)
#define REPEATB5(fn, basefn, ...) fn(5, ##__VA_ARGS__) REPEATB4(fn, basefn, ##__VA_ARGS__)
#define REPEATB6(fn, basefn, ...) fn(6, ##__VA_ARGS__) REPEATB5(fn, basefn, ##__VA_ARGS__)
#define REPEATB7(fn, basefn, ...) fn(7, ##__VA_ARGS__) REPEATB6(fn, basefn, ##__VA_ARGS__)
#define REPEATB8(fn, basefn, ...) fn(8, ##__VA_ARGS__) REPEATB7(fn, basefn, ##__VA_ARGS__)
#define REPEATB9(fn, basefn, ...) fn(9, ##__VA_ARGS__) REPEATB8(fn, basefn, ##__VA_ARGS__)
#define REPEATB10(fn, basefn, ...) fn(10, ##__VA_ARGS__) REPEATB9(fn, basefn, ##__VA_ARGS__)
#define REPEATB11(fn, basefn, ...) fn(11, ##__VA_ARGS__) REPEATB10(fn, basefn, ##__VA_ARGS__)
#define REPEATB12(fn, basefn, ...) fn(12, ##__VA_ARGS__) REPEATB11(fn, basefn, ##__VA_ARGS__)
#define REPEATB13(fn, basefn, ...) fn(13, ##__VA_ARGS__) REPEATB12(fn, basefn, ##__VA_ARGS__)
#define REPEATB14(fn, basefn, ...) fn(14, ##__VA_ARGS__) REPEATB13(fn, basefn, ##__VA_ARGS__)
#define REPEATB15(fn, basefn, ...) fn(15, ##__VA_ARGS__) REPEATB14(fn, basefn, ##__VA_ARGS__)
/* clang-format on */

/* 
 * REPEATB(n, fn, basefn, ...) 
 * REPEATB_REV(n, basefn, fn, ...)
 *
 * Macro to repeatedly apply a function or function-like macro `fn` a given
 * number of times, passing the index to each invocation. The passed index `n`
 * is first, followed by n-1 and so on. For the base case of 0, `basefn` is
 * applied instead of `fn`.
 *
 * REPEATB repeats from N to 0 (basefn is last). REPEATB_REV repeats from 0 to N
 * (basefn is first).
 */
#define REPEATB(n, fn, basefn, ...) REPEATB##n(fn, basefn, ##__VA_ARGS__)
#define REPEATB_REV(n, basefn, fn, ...) REPEATB_REV##n(fn, basefn, ##__VA_ARGS__)
/* Handy as the base-case for repeating from N to 1, excluding 1. */
#define nop_macro(x)

// TODO: Do we want to use sysconf(3) here?
#define PAGE_SIZE 4096

#define STACK_SIZE (4 * 1024 * 1024)

/* clang-format can't handle inline asm in macros */
/* clang-format off */
#if defined(__x86_64__)
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
#elif defined(__aarch64__)
#define _IA2_DEFINE_SIGNAL_HANDLER(function, tag)    \
    __asm__(".global ia2_sighandler_" #function "\n" \
            "ia2_sighandler_" #function ":\n"        \
            "movz_shifted_tag_x18 " #tag "\n"        \
            "b " #function "\n")
#endif
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

struct IA2SharedSection {
  const void *start;
  const void *end;
};

#define IA2_MAX_NUM_SHARED_SECTION_COUNT 4

// The data argument each time dl_iterate_phdr calls protect_pages
struct PhdrSearchArgs {
  // The compartment pkey to use when the segments are found
  int32_t pkey;

  // The address to search for while iterating through segments
  const void *address;

  // Other libraries to include in this compartment
  //
  // Semicolon separated list, terminated by NULL. May be NULL.
  const char *extra_libraries;

  // Number of other libraries from extra_libraries that were located and
  // protected.
  int found_library_count;

  // Array of shared section(s) to skip when protecting RW ranges. List is
  // terminated with a double NULL entry. List must be at most
  // IA2_MAX_SHARED_SECTION_COUNT elements (not including NULL terminating
  // pair). Pointer may be NULL if no shared ranges are used.
  const struct IA2SharedSection *shared_sections;
};

// The two following assembler macros are used because it's difficult to go from
// PKEYs/tags to PKRU/shifted tags in the preprocessor so we instead have the
// assembler do the calculation

// This emits the 5 bytes correponding to the movl $PKRU, %eax instruction
asm(".macro mov_pkru_eax pkey\n"
    ".byte 0xb8\n"
    ".long ~((3 << (2 * \\pkey)) | 3)\n"
    ".endm");

// Compare eax with the given PKRU mask
asm(".macro cmp_pkru_eax pkey\n"
    "cmpl $~((3 << (2 * \\pkey)) | 3), %eax\n"
    ".endm");

// This emits the 4 bytes corresponding to movz x18, $shifted_tag, lsl #48
asm(".macro movz_shifted_tag_x18 tag\n"
    ".byte 0x12\n"
    ".hword 0xe000 | (\\tag << 5)\n"
    ".byte 0xd2\n"
    ".endm");

// Obtain a string corresponding to errno in a threadsafe fashion.
#define errno_s (strerror_l(errno, uselocale((locale_t)0)))

#define PTRS_PER_PAGE (PAGE_SIZE / sizeof(void *))
#define IA2_MAX_THREADS (PTRS_PER_PAGE)

#define IA2_ROUND_DOWN(x, y) ((x) & ~((y)-1))

#if defined(__x86_64__)
/* clang-format can't handle inline asm in macros */
/* clang-format off */
/* Allocate and protect the stack for this thread's i'th compartment. */
#define ALLOCATE_COMPARTMENT_STACK_AND_SETUP_TLS(i)                            \
  {                                                                            \
    __IA2_UNUSED __attribute__((visibility ("default"))) extern __thread void *ia2_stackptr_##i;                       \
    __asm__ volatile(                                                          \
        /* write new pkru */                                                   \
        "xorl %%ecx, %%ecx\n"                                                  \
        "xorl %%edx, %%edx\n"                                                  \
        "mov_pkru_eax " #i "\n"                                                \
        "wrpkru\n"                                                             \
        :                                                                      \
        :                                                                      \
        : "rax", "rcx", "rdx");                                                \
                                                                               \
    register void *stack asm("rax") = allocate_stack(i);                       \
                                                                               \
    /* We must change the pkru to write the stack pointer because each */      \
    /* stack pointer is part of the compartment whose stack it points to. */   \
    __asm__ volatile(                                                          \
        "mov %0, %%r10\n"                                                      \
        /* save current rsp onto compartment stack */                          \
        "mov %%rsp,(%%r10)\n"                                                  \
        /* switch onto compartment stack */                                    \
        "mov %%r10,%%rsp\n"                                                    \
        /* align stack */                                                      \
        "sub $0x8,%%rsp\n"                                                     \
        /* run init_tls_i on the compartment's stack */                        \
        "call init_tls_" #i "\n"                                               \
        /* undo stack align */                                                 \
        "add $0x8,%%rsp\n"                                                     \
        /* switch to old stack and restore recently-alloc'd stack to r10 */    \
        "mov %%rsp,%%r10\n"                                                    \
        "mov (%%r10),%%rsp\n"                                                  \
        "mov ia2_stackptr_" #i "@GOTTPOFF(%%rip),%%r11\n"                      \
        /* check that stack pointer holds NULL */                              \
        "cmpq $0x0,%%fs:(%%r11)\n"                                             \
        "je .Lfresh_init%=" #i "\n"                                            \
        "mov $" #i ",%%rdi\n"                                                  \
        "call ia2_reinit_stack_err\n"                                          \
        "ud2\n"                                                                \
        /* %= produces a unique (to this inline asm block) label value */      \
        ".Lfresh_init%=" #i ":\n"                                              \
        /* store the stack addr in the stack pointer */                        \
        "mov %%r10,%%fs:(%%r11)\n"                                             \
        :                                                                      \
        : "rax"(stack)                                                         \
        : "rdi", "rcx", "rdx", "r10", "r11", "r12");                           \
  }
/* clang-format on */
#elif defined(__aarch64__)
#warning "ALLOCATE_COMPARTMENT_STACK_AND_SETUP_TLS does not do stackptr reinit checking"
#define ALLOCATE_COMPARTMENT_STACK_AND_SETUP_TLS(i)                            \
  {                                                                            \
    __IA2_UNUSED extern __thread void *ia2_stackptr_##i;                       \
                                                                               \
    register void *stack asm("x0") = allocate_stack(i);                        \
    __asm__ volatile(                                                          \
        "movz_shifted_tag_x18 " #i "\n"                                        \
        /* save old stack pointer */                                           \
        "mov x9, sp\n"                                                         \
        /* switch to newly allocated stack */                                  \
        "mov sp, %0\n"                                                         \
        /* push old stack pointer to new stack */                              \
        "str x9, [sp], #-8\n"                                                  \
        /* initialize TLS */                                                   \
        "bl init_tls_" #i "\n"                                                 \
        /* pop old stack pointer from new stack */                             \
        "ldr x9, [sp, #8]!\n"                                                  \
        /* save pointer to new stack */                                        \
        "mov x10, sp\n"                                                        \
        /* switch to old stack */                                              \
        "mov sp, x9\n"                                                         \
        /* calculate location to save pointer to newly allocated stack */      \
        "mrs x12, tpidr_el0\n"                                                 \
        "adrp x11, :gottprel:ia2_stackptr_" #i "\n"                            \
        "ldr x11, [x11, #:gottprel_lo12:ia2_stackptr_" #i "]\n"                \
        "add x11, x11, x12\n"                                                  \
        /* write newly allocated stack to ia2_stackptr_i */                    \
        "str x10, [x11]\n"                                                     \
        :                                                                      \
        : "r"(stack)                                                           \
        : "x9", "x10", "x11", "x12", "x19"                                     \
    );                                                                         \
  }
#endif

#if defined(__x86_64__)
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
#elif defined(__aarch64__)
#warning "libia2 does not implement return_stackptr_if_compartment yet"
#define return_stackptr_if_compartment(compartment)
#endif

/* Pass to mmap to signal end of program init */
#define IA2_FINISH_INIT_MAGIC 0x1a21face1a21faceULL
/* Tell the syscall filter to forbid init-only operations. This mmap() will
always fail because it maps a non-page-aligned addr with MAP_FIXED, so it
works as a reasonable signpost no-op. */
#define mark_init_finished() (void)mmap((void *)IA2_FINISH_INIT_MAGIC, 0, 0, MAP_FIXED, -1, 0)

#define declare_init_tls_fn(n) __attribute__((visibility("default"))) void init_tls_##n(void);
#define setup_destructors_for_compartment(n)                                   \
  __attribute__((visibility("default"))) void ia2_setup_destructors_##n(void);                                        \
  ia2_setup_destructors_##n();

#if defined(__aarch64__)
int ia2_mprotect_with_tag(void *addr, size_t len, int prot, int tag);
#elif defined(__x86_64__)
#if IA2_DEBUG_LOG
static int ia2_mprotect_with_tag(void *addr, size_t len, int prot, int tag) {
  printf("ia2_mprotect_with_tag(addr=%p, len=%zu, prot=%d, tag=%d)\n", addr, len, prot, tag);
  return pkey_mprotect(addr, len, prot, tag);
}
#else
/* We can't use an alias attribute since this points to a function outside the translation unit */
#define ia2_mprotect_with_tag pkey_mprotect
#endif
#endif
char *allocate_stack(int i);
void verify_tls_padding(void);
void ia2_set_up_tags(int *n_to_alloc);
__attribute__((__noreturn__)) void ia2_reinit_stack_err(int i);

/* clang-format can't handle inline asm in macros */
/* clang-format off */
#define PKRU_LABEL(i) , pkru##i
#define PKRU_LABEL_NO_COMMA(i) pkru##i
#if defined(__x86_64__)
#define CMP_AND_JMP(i)                                                         \
  "cmp_pkru_eax " #i "\n"                                                      \
  "je " IA2_STR(IA2_CONCAT(%l, i)) "\n"                                        \

#define BODY_AND_WRPKRU(i, body, max)                                          \
    pkru##i:                                                                   \
    do { body } while (0);                                                     \
    __asm__ volatile(                                                          \
        "xorl %%ecx, %%ecx\n"                                                  \
        "xorl %%edx, %%edx\n"                                                  \
        "mov_pkru_eax " #i "\n"                                                \
        "wrpkru\n"                                                             \
        :                                                                      \
        :                                                                      \
        : "rax", "rcx", "rdx");                                                \
    goto done;                                                                 \

#define COMPARTMENT_SAVE_AND_RESTORE(body, max)                                \
    __asm__ goto (                                                             \
        /* zero ecx as precondition of rdpkru */                               \
        "xor %%ecx,%%ecx\n"                                                    \
        /* eax = old pkru; also zeroes edx, which is required for wrpkru */    \
        "rdpkru\n"                                                             \
        /* compare eax against each valid PKRU (up to max) and jump to the */  \
        /* corresponding label defined by BODY_AND_WRPKRU(i, body) */          \
        REPEATB(max, CMP_AND_JMP, CMP_AND_JMP)                                 \
        /* If we get here, we have an unexpected PKRU value, default to 0. */  \
        "jmp %l0\n"                                                            \
        :                                                                      \
        :                                                                      \
        : "rax", "rcx", "rdx", "cc"                                            \
        : REPEATB_REV(max, PKRU_LABEL_NO_COMMA, PKRU_LABEL));                  \
    REPEATB(max, BODY_AND_WRPKRU, BODY_AND_WRPKRU, body, max)                  \
    done:

#elif defined(__aarch64__)
#define CMP_AND_JMP(i)                                                         \
  "cmp x19, " #i "\n"                                                          \
  "b.eq " IA2_STR(IA2_CONCAT(%l, i)) "\n"                                      \

#define BODY_AND_WRPKRU(i, body)                                               \
    pkru##i:                                                                   \
    do { body } while (0);                                                     \
    __asm__ volatile(                                                          \
        "movz_shifted_tag_x18 " #i "\n");                                      \
    goto done;                                                                 \

#define COMPARTMENT_SAVE_AND_RESTORE(body, max)                                \
    __asm__ goto (                                                             \
        "lsr x19, x18, #56\n"                                                  \
        /* compare x19 against each valid key (up to max) and jump to the */   \
        /* corresponding label defined by BODY_AND_WRPKRU(i, body) */          \
        REPEATB(max, CMP_AND_JMP, CMP_AND_JMP)                                 \
        /* If we get here, we have an unexpected PKRU value, default to 0. */  \
        "b %l0\n"                                                              \
        :                                                                      \
        :                                                                      \
        : "x19", "cc"                                                          \
        : REPEATB_REV(max, PKRU_LABEL_NO_COMMA, PKRU_LABEL));                  \
    REPEATB(max, BODY_AND_WRPKRU, BODY_AND_WRPKRU, body)                       \
    done:

#endif
/* clang-format on */

#define _IA2_INIT_RUNTIME(n)                                                   \
  __attribute__((visibility("default"))) int ia2_n_pkeys_to_alloc = n;                                                \
  __attribute__((visibility("default"))) __thread void *ia2_stackptr_0[PAGE_SIZE / sizeof(void *)]                    \
      __attribute__((aligned(4096)));                                          \
                                                                               \
  REPEATB(n, declare_init_tls_fn, nop_macro);                                  \
                                                                               \
  /* Returns `&ia2_stackptr_N` given a pkru value for the Nth compartment. */  \
  __attribute__((visibility("default"))) void **ia2_stackptr_for_pkru(uint32_t pkru) {                                \
    REPEATB(n, return_stackptr_if_compartment,                                 \
            return_stackptr_if_compartment);                                   \
    return NULL;                                                               \
  }                                                                            \
                                                                               \
  __attribute__((visibility("default"))) __attribute__((weak)) void init_stacks_and_setup_tls(void) {                 \
    verify_tls_padding();                                                      \
    COMPARTMENT_SAVE_AND_RESTORE(REPEATB(n, ALLOCATE_COMPARTMENT_STACK_AND_SETUP_TLS, nop_macro), n); \
    /* allocate an unprotected stack for the untrusted compartment */          \
    ia2_stackptr_0[0] = allocate_stack(0);                                     \
  }                                                                            \
                                                                               \
  __attribute__((constructor)) static void ia2_init(void) {                    \
    /* Set up global resources. */                                             \
    ia2_set_up_tags(&ia2_n_pkeys_to_alloc);                                    \
    /* Initialize stacks for the main thread/ */                               \
    init_stacks_and_setup_tls();                                               \
    REPEATB##n(setup_destructors_for_compartment, nop_macro);                  \
    mark_init_finished();                                                      \
  }
