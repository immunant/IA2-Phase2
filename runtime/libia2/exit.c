#include "ia2.h"
#include <dlfcn.h>

__attribute__((used))
static void call_libc_exit(int status) {
  void (*exit_ptr)(int) = dlsym(RTLD_NEXT, "exit");
  if (!exit_ptr) {
    printf("Could not find exit(3) in the next DSO\n");
    _exit(status);
  }
  exit_ptr(status);
}

__attribute__((naked)) void _exit(int status) {
  __asm__(
#if LIBIA2_X86_64
  "jmp exit\n"
#elif LIBIA2_AARCH64
  "b exit\n"
#endif
  );
}

__attribute__((naked)) void exit(int status) {
  __asm__(
#if LIBIA2_X86_64
      /* clang-format off */
      "pushq %%rbp\n"
      "movq %%rsp, %%rbp\n"
      // Load the stack pointer for the shared compartment's stack.
      "mov ia2_stackptr_0@GOTTPOFF(%%rip), %%r11\n"
      "mov %%fs:(%%r11), %%rsp\n"
      // Switch pkey to the appropriate compartment.
      "xor %%ecx,%%ecx\n"
      "mov %%ecx,%%edx\n"
      "mov_pkru_eax 0\n"
      "wrpkru\n"
      // Align the stack before continuing
      "subq $8, %%rsp\n"
      // Call the real exit function.
      "call call_libc_exit\n"
      /* clang-format on */
#elif LIBIA2_AARCH64
#warning "libia2 does not properly wrap `exit` yet"
      "udf #0\n"
#endif
      ::);
}
