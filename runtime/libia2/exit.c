#include "ia2.h"
#include <dlfcn.h>

__attribute__((used)) static void call_libc_exit(int status) {
  void (*exit_ptr)(int) = dlsym(RTLD_NEXT, "exit");
  if (!exit_ptr) {
    printf("Could not find exit(3) in the next DSO\n");
    _exit(status);
  }
  exit_ptr(status);
}

/* clang-format off */
void _exit(int status);

__asm__(
    ".global _exit\n"
    "_exit:\n"
#if defined(__x86_64__)
    "jmp exit\n"
#elif defined(__aarch64__)
    "b exit\n"
#endif
);

void exit(int status);

__asm__(
    ".global exit\n"
    "exit:\n"
#if defined(__x86_64__)
    "pushq %rbp\n"
    "movq %rsp, %rbp\n"
    // Load the stack pointer for the shared compartment's stack.
    "mov ia2_stackptr_0@GOTTPOFF(%rip), %r11\n"
    "mov %fs:(%r11), %rsp\n"
#ifdef IA2_LIBC_COMPARTMENT
    // Switch pkey to the exit/libc compartment (enables pkeys 0 and 1).
#else
    // Switch pkey to the appropriate compartment.
#endif
    "xor %ecx,%ecx\n"
    "mov %ecx,%edx\n"
#ifdef IA2_LIBC_COMPARTMENT
    "mov_pkru_eax 1\n"
#else
    "mov_pkru_eax 0\n"
#endif
    "wrpkru\n"
    // Align the stack before continuing
    "subq $8, %rsp\n"
    // Call the real exit function.
    "call call_libc_exit\n"
#elif defined(__aarch64__)
    "stp x29, x30, [sp, #-16]!\n"
    // Load the stack pointer for the shared compartment's stack.
    "mrs x9, tpidr_el0\n"
    "adrp x10, :gottprel:ia2_stackptr_0\n"
    "ldr x10, [x10, #:gottprel_lo12:ia2_stackptr_0]\n"
    "add x10, x10, x9\n"
    "ldr x10, [x10]\n"
    "mov sp, x10\n"

    // Set x18 tag to 0
    "movz x18, #0x0000, LSL #48\n"

    "bl call_libc_exit\n"
#endif
);
/* clang-format on */
