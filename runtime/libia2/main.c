#include "ia2.h"

int __real_main(int argc, char **argv);

/* Stores the stack pointer to return to after main() is called. */
static void *main_sp __attribute__((used)) = 0;

/* XXX: Assumes main compartment has pkey 1. */
__attribute__((naked)) int __wrap_main(int argc, char **argv) {
  __asm__(
#if LIBIA2_X86_64
      /* clang-format off */
      "pushq %%rbp\n"
      "movq %%rsp, %%rbp\n"
      // Save the old stack pointer in main_sp.
      "movq %%rsp, main_sp(%%rip)\n"
      // Load the stack pointer for this compartment's stack.
      "mov ia2_stackptr_1@GOTTPOFF(%%rip), %%r11\n"
      "mov %%fs:(%%r11), %%rsp\n"
      // Switch pkey to the appropriate compartment.
      "xor %%ecx,%%ecx\n"
      "mov %%ecx,%%edx\n"
      "mov_pkru_eax 1\n"
      "wrpkru\n"
      // Align the stack before calling main.
      "subq $8, %%rsp\n"
      // Call the real main function.
      "call __real_main\n"
      // Restore the old stack pointer before returning.
      "mov main_sp(%%rip), %%rsp\n"
      // Save return value
      "mov %%rax,%%r10\n"
      // Switch pkey to untrusted compartment
      "xor %%ecx,%%ecx\n"
      "xor %%edx,%%edx\n"
      "mov_pkru_eax 0\n"
      "wrpkru\n"
      // Restore return value
      "mov %%r10,%%rax\n"
      "popq %%rbp\n"
      "ret\n"
      /* clang-format on */
#elif LIBIA2_AARCH64
#warning "libia2 does not properly wrap `main` yet"
      "b __real_main\n"
#endif
      ::);
}
