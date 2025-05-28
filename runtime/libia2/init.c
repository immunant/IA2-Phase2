#include "ia2.h"
#include "ia2_internal.h"
#include "memory_maps.h"

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

#if IA2_DEBUG_MEMORY
  struct ia2_thread_metadata *const thread_metadata = ia2_thread_metadata_get_current_thread();
  if (thread_metadata) {
    thread_metadata->stack_addrs[i] = (uintptr_t)stack;
  }
#endif

#ifdef __aarch64__
  /* Tag the allocated stack pointer so it is accessed with the right pkey */
  stack = (char *)((uint64_t)stack | (uint64_t)i << 56);
#endif

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
static void ia2_set_up_tags(void) {
#if defined(__x86_64__)
  for (int pkey = 1; pkey < IA2_MAX_COMPARTMENTS; pkey++) {
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

static void ia2_protect_memory(const char *libs, int compartment, const char *extra_libraries) {
    ia2_log("protecting memory for compartment %d\n", compartment);
}

struct CompartmentConfig {
    const char *libs;
    const char *extra_libraries;
};

/*
 * Compartment 0 does not need to be protected so we don't need to store config info. This variable
 * will only be accessed from the compartment defining ia2_main since it's the only one that should
 * call ia2_register_compartment so other copies of it won't be referenced if libia2 is statically
 * linked into all compartments.
 */
static struct CompartmentConfig user_config[IA2_MAX_COMPARTMENTS - 1] = { 0 };

/*
 * Stores the main DSO and extra libraries (if any) for the specified compartment. This should only
 * be called for protected compartments (calls specifying compartment 0 are no-ops).
 */
void ia2_register_compartment(const char *libs, int compartment, const char *extra_libraries) {
    ia2_log("registered %s and %s for compartment #%d\n", libs, extra_libraries, compartment);
    assert(compartment < IA2_MAX_COMPARTMENTS);
    user_config[compartment].libs = libs;
    user_config[compartment].extra_libraries = extra_libraries;
}

/*
 * This function is called from __wrap_main before handing off control to user code. It calls
 * ia2_main which is the user-defined compartment config code.
 */
void ia2_start(void) {
    ia2_log("initializing ia2 runtime");
    /* Get the user config before doing anything else */
    ia2_main();
    /* Set up global resources. */
    ia2_set_up_tags();
    /* Check the config for compartments 1..=15 */
    for (int i = 1; i < IA2_MAX_COMPARTMENTS; i++) {
        /* Skip blank user_config entries */
        if (!user_config[i].libs) {
            /* Sanity check to ensure it wasn't misconfigured */
            assert(user_config[i].extra_libraries);
            continue;
        }
        ia2_protect_memory(user_config[i].libs, i, user_config[i].extra_libraries);
    }
}
