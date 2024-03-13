#include "ia2_internal.h"

/* The 0th compartment is unprivileged and does not protect its memory, */
/* so declare its stack pointer in the shared object that sets up the */
/* runtime. */
/* Ensure that ia2_stackptr_0 is at least a page long to ensure that the */
/* last page of the TLS segment of compartment 0 does not contain any */
/* variables that will be used, because the last page-1 bytes may be */
/* ia2_mprotect_with_taged by the next compartment depending on sizes/alignment. */
extern __thread void *ia2_stackptr_0[PAGE_SIZE / sizeof(void *)]
    __attribute__((aligned(4096)));

/* Allocate a fixed-size stack and protect it with the ith pkey. */
/* Returns the top of the stack, not the base address of the allocation. */
char *allocate_stack(int i) {
  char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANON, -1, 0);
  if (stack == MAP_FAILED) {
    printf("Failed to allocate stack %d (%s)\n", i, errno_s);
    exit(-1);
  }
  if (i != 0) {
    int res = ia2_mprotect_with_tag(stack, STACK_SIZE, PROT_READ | PROT_WRITE, i);
    if (res == -1) {
      printf("Failed to mprotect stack %d (%s)\n", i, errno_s);
      exit(-1);
    }
  }
  /* Each stack frame start + 8 is initially 16-byte aligned. */
  return stack + STACK_SIZE - 8;
}

/* Confirm that stack pointers for compartments 0 and 1 are on separate */
/* pages. */
void verify_tls_padding(void) {
  /* It's safe to depend on ia2_stackptr_1 existing because all users of */
  /* IA2 will have at least one compartment other than the untrusted one. */
  extern __thread void *ia2_stackptr_1;
  if (IA2_ROUND_DOWN((uintptr_t)&ia2_stackptr_1, PAGE_SIZE) ==
      IA2_ROUND_DOWN((uintptr_t)ia2_stackptr_0, PAGE_SIZE)) {
    printf("ia2_stackptr_1 is too close to ia2_stackptr_0\n");
    exit(1);
  }
}

/* Ensure that all required pkeys are allocated or no-op on aarch64. */
void ensure_pkeys_allocated(int *n_to_alloc) {
#if LIBIA2_X86_64
  if (*n_to_alloc != 0) {
    for (int pkey = 1; pkey <= *n_to_alloc; pkey++) {
      int allocated = pkey_alloc(0, 0);
      if (allocated < 0) {
        printf("Failed to allocate protection key %d (%s)\n", pkey,
               errno_s);
        exit(-1);
      }
      if (allocated != pkey) {
        printf(
            "Failed to allocate protection keys in the expected order\n");
        exit(-1);
      }
    }
    *n_to_alloc = 0;
  }
#endif
}

/* Forbid overwriting an existing stack. */
_Noreturn void ia2_reinit_stack_err(int i) {
  printf("compartment %d in thread %d tried to allocate existing stack\n",
         i, gettid());
  exit(1);
}
