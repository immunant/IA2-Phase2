#include "ia2.h"

int __real_main(int argc, char **argv);

/* Stores the stack pointer to return to after main() is called. */
static void *main_sp __attribute__((used)) = 0;

__attribute__((naked)) int __wrap_main(int argc, char **argv) {
  __asm__(
      "pushq %%rbp\n"
      "movq %%rsp, %%rbp\n"
      // Save the old stack pointer in main_sp.
      "movq %%rsp, main_sp(%%rip)\n"
      // Load the stack pointer for this compartment's stack.
      "mov ia2_stackptr_1@GOTTPOFF(%%rip), %%r11\n"
      "mov %%fs:(%%r11), %%rsp\n"
      // Align the stack before calling main.
      "subq $8, %%rsp\n"
      // Call the real main function.
      "call __real_main\n"
      // Restore the old stack pointer before returning.
      "mov main_sp(%%rip), %%rsp\n"
      "popq %%rbp\n"
      "ret\n" ::);
  return 0;
}
