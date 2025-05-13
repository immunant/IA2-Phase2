#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <elf.h>
#include "ia2.h"
#include "ia2_internal.h"
#include <sys/auxv.h>
#include <sys/prctl.h>

/* Pass to mmap to signal end of program init */
#define IA2_FINISH_INIT_MAGIC 0x1a21face1a21faceULL

static void ia2_set_up_tags(void);
static void verify_tls_padding(void);
static void allocate_stack_0();

/* The 0th compartment is unprivileged and does not protect its memory, */
/* so declare its stack pointer in the shared object that sets up the */
/* runtime. */
/* Ensure that ia2_stackptr_0 is at least a page long to ensure that the */
/* last page of the TLS segment of compartment 0 does not contain any */
/* variables that will be used, because the last page-1 bytes may be */
/* ia2_mprotect_with_taged by the next compartment depending on sizes/alignment. */
// TODO: this may need to move depending on how we intend to link in this .o
__attribute__((visibility("default"))) __thread void *ia2_stackptr_0[PAGE_SIZE / sizeof(void *)] __attribute__((aligned(4096)));

void ia2_setup_destructors(const Elf64_Ehdr *ehdr, int pkey, void *wrap_ia2_compartment_destructor_arg, void *compartment_destructor_ptr_arg, struct FinalizerInfo *finalizers);

void ia2_start(void) {
    ia2_set_up_tags();
    verify_tls_padding();
    allocate_stack_0();
    ia2_main();
    /* Tell the syscall filter to forbid init-only operations. This mmap() will
    always fail because it maps a non-page-aligned addr with MAP_FIXED, so it
    works as a reasonable signpost no-op. */
    mmap((void *)IA2_FINISH_INIT_MAGIC, 0, 0, MAP_FIXED, -1, 0);
}

void ia2_protect_memory(const char *libs, int compartment, const char *extra_libraries) {
    void *initial_sp = allocate_stack(compartment);
    // TODO: this approach won't work so we may need to define a function in each compartment to get
    // the current thread's stack pointer
    extern __thread void *ia2_stackptr_1;
    extern __thread void *ia2_stackptr_2;
    if (compartment == 1) {
        ia2_stackptr_1 = initial_sp;
    } else if (compartment == 2) {
        ia2_stackptr_2 = initial_sp;
    }
    printf("%s: protecting %s with pkey %d\n", __func__, libs, compartment);

    // TODO: split up libs by whitespace

    void *dso_addr = NULL;
    void *dso_shared_start = NULL;
    void *dso_shared_stop = NULL;
    // Get the address of things defined by ia2_compartment_init.inc.
    // TODO: it may be possible to reduce the number of variables we have to look up by consolidating some of these definitions.
    void *x = NULL;
    void *y = NULL;
    void *z = NULL;

    if (!strcmp(libs, "main")) {
        dso_addr = dlsym(RTLD_DEFAULT, "main");
        dso_shared_start = dlsym(RTLD_DEFAULT, "__start_ia2_shared_data");
        dso_shared_stop = dlsym(RTLD_DEFAULT, "__stop_ia2_shared_data");
        x = dlsym(RTLD_DEFAULT, "__wrap_ia2_compartment_destructor_1");
        y = dlsym(RTLD_DEFAULT, "compartment_destructor_ptr_1");
        z = dlsym(RTLD_DEFAULT, "finalizers_1");
    } else {
        void *handle = dlopen(libs, RTLD_GLOBAL | RTLD_NOW);
        if (!handle) {
            printf("%s: failed to dlopen %s for compartment %d\n", __func__, libs, compartment);
            // TODO: use actual error codes
            exit(-1);
        }
        // TODO: use a symbol that will always be defined. This is specific to tests/two_keys_minimal/plugin.c
        dso_addr = dlsym(handle, "start_plugin");
        dso_shared_start = dlsym(handle, "__start_ia2_shared_data");
        dso_shared_stop = dlsym(handle, "__stop_ia2_shared_data");
        x = dlsym(handle, "__wrap_ia2_compartment_destructor_2");
        y = dlsym(handle, "compartment_destructor_ptr_2");
        z = dlsym(handle, "finalizers_2");
    }
    if (!dso_addr) {
        printf("%s: failed to dlsym symbol 'foo' in %s for compartment %d\n", __func__, libs, compartment);
        exit(-2);
    }
    if (!dso_shared_stop != !dso_shared_start) {
        // We should not have one be null without the other
        exit(-3);
    }
    assert(x);
    assert(y);
    assert(z);
    struct IA2SharedSection shared_sections[2] = {
        { dso_shared_start, dso_shared_stop },
        {NULL, NULL},
    };
    struct PhdrSearchArgs args = {
        .pkey = compartment,
        .address = dso_addr,
        .extra_libraries = extra_libraries,
        .found_library_count = 0,
        .shared_sections = shared_sections,
        .ehdr = NULL,
    };
    dl_iterate_phdr(protect_pages, &args);
    dl_iterate_phdr(protect_tls_pages, &args);
    ia2_setup_destructors(args.ehdr, compartment, x, y, z);
    /* Check that we found all extra libraries */
    const char *cur_pos = args.extra_libraries;
    int extra_library_count = 0;
    while (cur_pos) {
      extra_library_count++;
      cur_pos = strchr(cur_pos, ';');
      if (cur_pos) {
        cur_pos++;
      }
    }
    if (extra_library_count != args.found_library_count) {
      fprintf(
          stderr,
          "WARNING: Not all libraries in IA2_COMPARTMENT_LIBRARIES were found.\n");
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
#ifdef __aarch64__
  return stack + STACK_SIZE - 16;
#elif defined(__x86_64__)
  /* Each stack frame start + 8 is initially 16-byte aligned. */
  return stack + STACK_SIZE - 8;
#endif
}

static void allocate_stack_0() {
  ia2_stackptr_0[0] = allocate_stack(0);
}

/* Confirm that stack pointers for compartments 0 and 1 are on separate */
/* pages. */
static void verify_tls_padding(void) {
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
    // TODO: Add a macro for this
    for (int pkey = 1; pkey <= 15; pkey++) {
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
