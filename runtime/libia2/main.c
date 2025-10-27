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

    // Call ia2_start making sure to preserve/restore the original arguments to main
    "pushq %rdi\n"
    "pushq %rsi\n"
    "pushq %rdx\n"
    "subq $8, %rsp\n"
    "callq ia2_start\n"
    "addq $8, %rsp\n"
    "popq %rdx\n"
    "popq %rsi\n"
    "popq %rdi\n"

    // Switch pkey to the appropriate compartment.
    "xor %ecx,%ecx\n"
    "mov %ecx,%edx\n"
    "mov_pkru_eax 1\n"
    "wrpkru\n"
    // Save the old stack pointer in main_sp.
    "movq %rsp, main_sp(%rip)\n"
    // Load the stack pointer for this compartment's stack.
    "mov ia2_stackptr_1@GOTTPOFF(%rip), %r11\n"
    "mov %fs:(%r11), %rsp\n"
    // Align the stack before calling main.
    "subq $8, %rsp\n"
    // Call the real main function.
    "call __real_main\n"
    // Restore the old stack pointer before returning.
    // __libc_start_main owns the frame that called us, so it expects to see the
    // exact stack pointer it established when __wrap_main returns. We therefore
    // switch back to the saved kernel-provided stack instead of staying on the
    // compartment stack.
    "mov main_sp(%rip), %rsp\n"
    // Save return value
    "mov %rax,%r10\n"
    // NOTE: Removed switch to compartment 0 to allow exit handlers to run
    // in compartment 1 (where libc lives). This prevents SEGV_PKUERR when
    // exit() tries to acquire __exit_funcs_lock in libc's .bss section.
    // "xor %ecx,%ecx\n"
    // "xor %edx,%edx\n"
    // "mov_pkru_eax 0\n"
    // "wrpkru\n"
    // Leaving PKRU set to compartment 1 is safe because PKRU(1) still permits
    // access to pkey 0, so the restored stack (tagged with pkey 0) remains
    // readable and writable during libc teardown.
    // Restore return value
    "mov %r10,%rax\n"
    "popq %rbp\n"
    "ret\n"
#elif defined(__aarch64__)
    // prologue
    "stp x29, x30, [sp, #-16]!\n"
    "mov x29, sp\n"

    // Call ia2_start making sure to preserve/restore the original arguments to main
    "stp x0, x1, [sp, #-16]!\n"
    "bl ia2_start\n"
    "ldp x0, x1, [sp], #16\n"

    // Save old stack pointer in main_sp
    "adrp x9, main_sp\n"
    "add x9, x9, #:lo12:main_sp\n"
    // Tag x9 with compartment 1
    "orr x9, x9, #0x100000000000000\n"

    "str x29, [x9]\n"

    // Load the new stack pointer
    // Since this accesses a TLS in the same DSO it's simpler than the TLS reference in ia2_internal.h
    "mrs x9, tpidr_el0\n"
    "add x9, x9, #:tprel_hi12:ia2_stackptr_1\n"
    "add x9, x9, #:tprel_lo12_nc:ia2_stackptr_1\n"

    // Tag x9 with compartment 1
    "orr x9, x9, #0x100000000000000\n"

    "ldr x9, [x9]\n"
    "mov sp, x9\n"

    // Set x18 tag to 1
    "movz_shifted_tag_x18 1\n"

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

    "ldp x29, x30, [sp], #16\n"
    "ret"
#endif
);
/* clang-format on */
