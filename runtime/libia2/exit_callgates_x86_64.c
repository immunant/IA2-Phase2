#include "ia2.h"

#if defined(__x86_64__)

// Exit compartment is hardcoded to pkey 1 (where libc/ld.so live)
#define IA2_EXIT_COMPARTMENT_PKEY 1

// PKRU mask for the exit compartment: allow access to pkey 0 and pkey 1 only.
// PKRU uses 2 bits per protection key (Access Disable, Write Disable).
// Result: 0xfffffff0 (pkey 0 and 1 accessible, pkeys 2-15 blocked)
#define IA2_EXIT_PKRU (~((3U << (2 * IA2_EXIT_COMPARTMENT_PKEY)) | 3U))

// Stringify helper macros for converting macro values to assembly immediates
#define STR(x) #x
#define XSTR(x) STR(x)

__asm__(
    ".text\n"
    ".p2align 4,,15\n"
    ".globl ia2_callgate_enter\n"
    ".type ia2_callgate_enter,@function\n"
"ia2_callgate_enter:\n"
    "subq $24, %rsp\n"

    "xorl %ecx, %ecx\n"
    "xorl %edx, %edx\n"
    "rdpkru\n"
    "movl %eax, 0(%rsp)\n"              // save caller PKRU
    "leaq 24(%rsp), %r9\n"
    "movq %r9, 8(%rsp)\n"               // save caller stack pointer

    "movl $" XSTR(IA2_EXIT_COMPARTMENT_PKEY) ", %edi\n"
    "call ia2_stackptr_for_compartment@PLT\n"

    "movq (%rax), %r10\n"               // load exit stack pointer
    "testq %r10, %r10\n"
    "jne 1f\n"
    "addq $24, %rsp\n"
    "ud2\n"                             // trap if exit stack uninitialized
"1:\n"
    "movl 0(%rsp), %r8d\n"              // reload saved PKRU
    "movq 8(%rsp), %r9\n"               // reload saved SP
    "addq $24, %rsp\n"

    "movl $" XSTR(IA2_EXIT_PKRU) ", %r11d\n"
    "movl %r11d, %eax\n"
    "xorl %ecx, %ecx\n"
    "xorl %edx, %edx\n"
    "wrpkru\n"

    ASSERT_PKRU(IA2_EXIT_PKRU)

    "movq %r10, %rsp\n"                 // switch to exit stack
    "movq %r9, %rdx\n"                  // return saved SP
    "movl %r8d, %eax\n"                 // return saved PKRU
    "ret\n"

    ".globl ia2_callgate_exit\n"
    ".type ia2_callgate_exit,@function\n"
"ia2_callgate_exit:\n"
    "testq %rsi, %rsi\n"
    "je 3f\n"
    "movq %rsi, %rsp\n"
"3:\n"
    "movl %edi, %eax\n"
    "xorl %ecx, %ecx\n"
    "xorl %edx, %edx\n"
    "wrpkru\n"

    ASSERT_PKRU(%edi)

    "ret\n"

    ".size ia2_callgate_enter, .-ia2_callgate_enter\n"
    ".size ia2_callgate_exit, .-ia2_callgate_exit\n"
);

#endif // defined(__x86_64__)
