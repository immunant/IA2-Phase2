#ifdef IA2_LIBC_COMPARTMENT

// -----------------------------------------------------------------------------
// Exit call gates
// -----------------------------------------------------------------------------
//
// The x86_64 implementation of exit call gates is provided in inline assembly
// to avoid stack corruption bugs that occur when modifying %rsp in C code.
//
// The __wrap___cxa_finalize function performs PKRU/stack transitions when
// entering the exit compartment (pkey 1, where libc/ld.so live).
//
// For other architectures, an assembly implementation must be provided.

#include "ia2.h"
#include "ia2_internal.h"
#include "ia2_compartment_ids.h"

#if defined(__x86_64__)

// PKRU mask for the exit compartment: allow access to pkey 0 and pkey 1 only.
// PKRU uses 2 bits per protection key (Access Disable, Write Disable).
// Result: PKRU(1) == 0xfffffff0 (pkey 0 and 1 accessible, pkeys 2-15 blocked)
#define IA2_EXIT_PKRU PKRU(IA2_LIBC_COMPARTMENT)

__asm__(
    ".text\n"
    ".globl __wrap___cxa_finalize\n"
    ".type __wrap___cxa_finalize,@function\n"
"__wrap___cxa_finalize:\n"
    // Save callee-saved registers
    "pushq %rbp\n"
    "movq %rsp, %rbp\n"
    "pushq %rbx\n"
    "pushq %r12\n"
    "pushq %r13\n"
    "pushq %r14\n"
    "pushq %r15\n"
    "movq %rdi, %r12\n"                // preserve dso_handle

    // Inline ia2_callgate_enter
    "movq %rsp, %r9\n"                 // cache caller stack pointer for cookie
    "pushq %rbp\n"
    "movq %rsp, %rbp\n"
    "subq $32, %rsp\n"                 // reserve cookie storage and keep 16 byte alignment

    "movq (%r9), %rdi\n"               // grab caller return address before PKRU switch
    "movq %rdi, 16(%rsp)\n"            // stash for later push onto exit stack

    "xorl %ecx, %ecx\n"
    "xorl %edx, %edx\n"
    "rdpkru\n"
    "movl %eax, 0(%rsp)\n"             // save caller PKRU
    "movq %r9, 8(%rsp)\n"              // save caller stack pointer

    "movl $" IA2_STR(IA2_LIBC_COMPARTMENT) ", %edi\n"
    "call ia2_stackptr_for_compartment@PLT\n"

    "movq (%rax), %r10\n"              // load exit stack pointer
    "testq %r10, %r10\n"
    "jne 1f\n"
    "addq $32, %rsp\n"
    "popq %rbp\n"
    "ud2\n"                            // trap if exit stack uninitialized
"1:\n"
    "movl 0(%rsp), %r13d\n"            // reload saved PKRU -> r13d
    "movq 8(%rsp), %r14\n"             // reload saved SP -> r14
    "movq 16(%rsp), %rdi\n"            // reload caller return address
    "addq $32, %rsp\n"

    "popq %rbp\n"

    "movl $" IA2_STR(IA2_EXIT_PKRU) ", %r11d\n"
    "movl %r11d, %eax\n"
    "xorl %ecx, %ecx\n"
    "xorl %edx, %edx\n"
    "wrpkru\n"

    ASSERT_PKRU(IA2_EXIT_PKRU)

    "movq %r10, %rsp\n"                // switch to exit stack
    "pushq %rdi\n"                     // install caller return address on exit stack

    // Call __real___cxa_finalize with preserved dso_handle
    "movq %r12, %rdi\n"
    "subq $8, %rsp\n"                  // maintain 16-byte alignment
    "call __real___cxa_finalize@PLT\n"
    "addq $8, %rsp\n"

    // Inline ia2_callgate_exit
    "testq %r14, %r14\n"               // check saved SP
    "je 3f\n"
    "movq %r14, %rsp\n"                // restore stack pointer
"3:\n"
    "movl %r13d, %eax\n"               // restore PKRU from saved value
    "xorl %ecx, %ecx\n"
    "xorl %edx, %edx\n"
    "wrpkru\n"

    // Restore callee-saved registers and return
    "popq %r15\n"
    "popq %r14\n"
    "popq %r13\n"
    "popq %r12\n"
    "popq %rbx\n"
    "popq %rbp\n"
    "ret\n"

    ".size __wrap___cxa_finalize, .-__wrap___cxa_finalize\n"
);

#endif // defined(__x86_64__)

#endif // IA2_LIBC_COMPARTMENT
