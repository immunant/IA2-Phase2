#include "ia2.h"

int __real_main(int argc, char **argv);

/* Stores the stack pointer to return to after main() is called. */
static void *main_sp __attribute__((used)) = 0;

/* XXX: Assumes main compartment has pkey 1. */
/* clang-format off */
int __wrap_main(int argc, char **argv);
__asm__(
    ".text\n"
    ".global __wrap_main\n"
    "__wrap_main:\n"
#if defined(__x86_64__)
    "pushq %rbp\n"
    "movq %rsp, %rbp\n"
    // Push an arbitrary scratch register which will be preserved across
    // function calls
    "pushq %r12\n"
    "pushq %r13\n"
    "pushq %r14\n"

    // Call ia2_start making sure to preserve/restore the original arguments to main
    "pushq %rdi\n"
    "pushq %rsi\n"
    "pushq %rdx\n"
    // This sets rax to the initial PKRU
    "callq ia2_start\n"
    "popq %rdx\n"
    "popq %rsi\n"
    "popq %rdi\n"

    // Switch pkey to the appropriate compartment.
    "xor %ecx,%ecx\n"
    "mov %ecx,%edx\n"
    // rax set by previous call to ia2_start
    "wrpkru\n"

    // Save the old stack pointer in main_sp.
    "movq %rsp, main_sp(%rip)\n"

    // Load the stack pointer for this compartment's stack
    // We need to use %rdi to call ia2_stackptr_for_tag while preserving the
    // %rdi, %rsi and %rdx passed to __wrap_main but we can't use the stack
    // since we're switching it so let's save it in %r12-%r14 instead
    "movq %rdi, %r12\n"
    "movq %rsi, %r13\n"
    "movq %rdx, %r14\n"
    // Set the argument to ia2_stackptr_for_tag
    "movq %rax, %rdi\n"
    "subq $8, %rsp\n"
    "callq ia2_stackptr_for_tag\n"
    "addq $8, %rsp\n"
    // Set the stack pointer from the return value
    "movq %rax, %rsp\n"
    // Restore the arguments to main
    "movq %r12, %rdi\n"
    "movq %r13, %rsi\n"
    "movq %r14, %rdx\n"

    // Call the real main function.
    "call __real_main\n"

    // Restore the old stack pointer before returning.
    "mov main_sp(%rip), %rsp\n"
    // Save return value
    "mov %rax,%r10\n"
    // Switch pkey to untrusted compartment
    "xor %ecx,%ecx\n"
    "xor %edx,%edx\n"
    "mov_pkru_eax 0\n"
    "wrpkru\n"
    // Restore return value
    "mov %r10,%rax\n"
    "popq %r14\n"
    "popq %r13\n"
    "popq %r12\n"
    "popq %rbp\n"
    "ret\n"
#elif defined(__aarch64__)
    // prologue
    "stp x29, x30, [sp, #-16]!\n"
    "mov x29, sp\n"

    // Push arbitrary scratch registers which will be preserved across functions
    "stp x19, x20, [sp, #-16]!\n"
    "stp x21, x22, [sp, #-16]!\n"

    // Call ia2_start making sure to preserve/restore the original arguments to main
    "stp x0, x1, [sp, #-16]!\n"
    // This returns the initial x18 value in x0
    "bl ia2_start\n"
    "mov x18, x0\n"
    "ldp x0, x1, [sp], #16\n"

    // Save old stack pointer in main_sp
    "adrp x9, main_sp\n"
    "add x9, x9, #:lo12:main_sp\n"
    // Tag x9 with compartment 1
    "orr x9, x9, #0x100000000000000\n"

    "str x29, [x9]\n"

    // Load the new stack pointer for this compartment's stack
    // We need to use x0 to call ia2_stackptr_for_tag while preserving the
    // x0-x2 passed to __wrap_main but we can't use the stack since we're
    // switching it so let's save it in x19-x21 instead
    "mov x19, x0\n"
    "mov x20, x1\n"
    "mov x21, x2\n"
    // Set the argument to ia2_stackptr_for_tag
    "mov x0, x18\n"
    "bl ia2_stackptr_for_tag\n"
    // Set the stack pointer from the return value
    // Tag x0 with compartment
    "orr x0, x0, x18\n"

    "mov sp, x0\n"

    // Restore the arguments to main
    "mov x0, x19\n"
    "mov x1, x20\n"
    "mov x2, x21\n"

    // Call the real main function
    "bl __real_main\n"

    // Set x18 tag to 0
    "movz_shifted_tag_x18 0\n"

    // Restore the old stack pointer
    "adrp x9, main_sp\n"
    "add x9, x9, #:lo12:main_sp\n"

    // Tag x9 with compartment 1
    "orr x9, x9, #0x100000000000000\n"

    "ldr x9, [x9]\n"
    "mov sp, x9\n"

    "ldp x21, x22, [sp], #16\n"
    "ldp x19, x20, [sp], #16\n"
    "ldp x29, x30, [sp], #16\n"
    "ret"
#endif
);
/* clang-format on */
