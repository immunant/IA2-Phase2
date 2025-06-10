#include "ia2.h"
#include "ia2_internal.h"
#include "memory_maps.h"

#include <pthread.h>
#include <sys/auxv.h>
#include <sys/prctl.h>

/* The 0th compartment is unprivileged and does not protect its memory, */
/* so declare its stack pointer in the shared object that sets up the */
/* runtime. */
/* Ensure that ia2_stackptr_0 is at least a page long to ensure that the */
/* last page of the TLS segment of compartment 0 does not contain any */
/* variables that will be used, because the last page-1 bytes may be */
/* ia2_mprotect_with_taged by the next compartment depending on sizes/alignment. */
extern __thread void *ia2_stackptr_0[PAGE_SIZE / sizeof(void *)]
    __attribute__((aligned(4096)));

static pthread_key_t thread_stacks_key IA2_SHARED_DATA;

__thread void *stacks[IA2_MAX_COMPARTMENTS] = {0};

void thread_stacks_destructor(void *_unused) {
  for (size_t compartment = 0; compartment < IA2_MAX_COMPARTMENTS; compartment++) {
    void *const stack = stacks[compartment];
    if (!stack) {
      continue;
    }
    if (munmap(stacks[compartment], STACK_SIZE) == -1) {
      fprintf(stderr, "munmap failed\n");
      abort();
    }
  }
}

void create_thread_keys(void) {
  if (pthread_key_create(&thread_stacks_key, thread_stacks_destructor) != 0) {
    fprintf(stderr, "pthread_key_create failed\n");
    abort();
  }
}

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
#ifdef __aarch64__
  /* Tag the allocated stack pointer so it is accessed with the right pkey */
  stack = (char *)((uint64_t)stack | (uint64_t)i << 56);
#endif

#if IA2_DEBUG_MEMORY
  struct ia2_thread_metadata *const thread_metadata = ia2_thread_metadata_get_for_current_thread();
  // Atomic write.
  thread_metadata->stack_addrs[i] = (uintptr_t)stack;
#endif
  stacks[i] = stack;
  // The value set doesn't matter here as long as it's non-`NULL`.
  // `allocate_stack` is called for each compartment
  // when a new thread is created, so we just need to set a non-`NULL` value
  // (which triggers a destructor running on thread termination),
  // but it doesn't really matter what value it is,
  // since the destructor `thread_stacks_destructor`
  // just uses the TLS global `stack_ptrs` directly.
  if (pthread_setspecific(thread_stacks_key, (void *)stacks) != 0) {
    fprintf(stderr, "pthread_setspecific failed\n");
    abort();
  }

#ifdef __aarch64__
  return stack + STACK_SIZE - 16;
#elif defined(__x86_64__)
  /* Each stack frame start + 8 is initially 16-byte aligned. */
  return stack + STACK_SIZE - 8;
#endif
}

void allocate_stack_0() {
  ia2_stackptr_0[0] = allocate_stack(0);
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

/* Allocates the required pkeys on x86 or enables MTE on aarch64 */
void ia2_set_up_tags(int *n_to_alloc) {
#if defined(__x86_64__)
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
#elif defined(__aarch64__)
  if (!(getauxval(AT_HWCAP2) & HWCAP2_MTE)) {
    printf("MTE is not supported\n");
    exit(-1);
  }
  int res = prctl(PR_SET_TAGGED_ADDR_CTRL,
                  PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC | (0xFFFE << PR_MTE_TAG_SHIFT),
                  0, 0, 0);
  if (res) {
    printf("prctl(PR_SET_TAGGED_ADDR_CTRL) failed to enable MTE: (%s)\n", errno_s);
    exit(-1);
  }
#endif
}

/* Forbid overwriting an existing stack. */
__attribute__((__noreturn__)) void ia2_reinit_stack_err(int i) {
  printf("compartment %d in thread %d tried to allocate existing stack\n",
         i, gettid());
  exit(1);
}
