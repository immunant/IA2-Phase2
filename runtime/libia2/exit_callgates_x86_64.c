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
    "movq %rsp, %r9\n"               // cache caller stack pointer for cookie
    "pushq %rbp\n"
    "movq %rsp, %rbp\n"
    "subq $32, %rsp\n"               // reserve cookie storage and keep 16 byte alignment

    "movq (%r9), %rdi\n"             // grab caller return address before PKRU switch
    "movq %rdi, 16(%rsp)\n"          // stash for later push onto exit stack

    "xorl %ecx, %ecx\n"
    "xorl %edx, %edx\n"
    "rdpkru\n"
    "movl %eax, 0(%rsp)\n"              // save caller PKRU
    "movq %r9, 8(%rsp)\n"               // save caller stack pointer

    "movl $" XSTR(IA2_EXIT_COMPARTMENT_PKEY) ", %edi\n"
    "call ia2_stackptr_for_compartment@PLT\n"

    "movq (%rax), %r10\n"               // load exit stack pointer
    "testq %r10, %r10\n"
    "jne 1f\n"
    "addq $32, %rsp\n"
    "popq %rbp\n"
    "ud2\n"                             // trap if exit stack uninitialized
"1:\n"
    "movl 0(%rsp), %r8d\n"              // reload saved PKRU
    "movq 8(%rsp), %r9\n"               // reload saved SP
    "movq 16(%rsp), %rdi\n"             // reload caller return address
    "addq $32, %rsp\n"

    "popq %rbp\n"

    "movl $" XSTR(IA2_EXIT_PKRU) ", %r11d\n"
    "movl %r11d, %eax\n"
    "xorl %ecx, %ecx\n"
    "xorl %edx, %edx\n"
    "wrpkru\n"

    ASSERT_PKRU(IA2_EXIT_PKRU)

    "movq %r10, %rsp\n"                 // switch to exit stack
    "pushq %rdi\n"                       // install caller return address on exit stack
    "movq %r9, %rdx\n"                  // return saved SP
    "movl %r8d, %eax\n"                 // return saved PKRU
    "ret\n"

    ".size ia2_callgate_enter, .-ia2_callgate_enter\n"

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

    /*
    Removed ASSERT_PKRU(%edi) because of this error?
    FAILED: runtime/libia2/CMakeFiles/libia2.dir/exit_callgates_x86_64.c.o
            /tmp/ccVvafxZ.s: Assembler messages:
            /tmp/ccVvafxZ.s:82: Error: illegal immediate register operand %edi
    */
#ifdef IA2_DEBUG
    "movq %rcx, %r10\n"             // save rcx (clobbered by rdpkru)
    "rdpkru\n"
    "cmpl %edi, %eax\n"             // compare requested PKRU vs actual
    "je 2f\n"
    "ud2\n"                         // trap if mismatch
"2:\n"
    "movq %r10, %rcx\n"             // restore rcx
#endif

    "ret\n"

    ".size ia2_callgate_exit, .-ia2_callgate_exit\n"

    ".globl __wrap___cxa_finalize\n"
    ".type __wrap___cxa_finalize,@function\n"
"__wrap___cxa_finalize:\n"
    "pushq %rbp\n"
    "movq %rsp, %rbp\n"
    "pushq %rbx\n"
    "pushq %r12\n"
    "pushq %r13\n"
    "pushq %r14\n"
    "pushq %r15\n"
    "movq %rdi, %r12\n"                // preserve dso_handle
    "call ia2_callgate_enter\n"
    "movl %eax, %r13d\n"               // saved PKRU
    "movq %rdx, %r14\n"                // saved SP
    "movq %r12, %rdi\n"
    "subq $8, %rsp\n"                  // maintain 16-byte alignment
    "call __real___cxa_finalize@PLT\n"
    "addq $8, %rsp\n"
    "movl %r13d, %edi\n"
    "movq %r14, %rsi\n"
    "subq $8, %rsp\n"
    "call ia2_callgate_exit\n"
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
