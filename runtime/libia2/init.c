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

/* The 0th compartment is unprivileged and does not protect its memory, */
/* so declare its stack pointer in the shared object that sets up the */
/* runtime. */
/* Ensure that ia2_stackptr_0 is at least a page long to ensure that the */
/* last page of the TLS segment of compartment 0 does not contain any */
/* variables that will be used, because the last page-1 bytes may be */
/* ia2_mprotect_with_taged by the next compartment depending on sizes/alignment. */
// TODO: this may need to move depending on how we intend to link in this .o
__attribute__((visibility("default"))) __thread void *ia2_stackptr_0[PAGE_SIZE / sizeof(void *)] __attribute__((aligned(4096)));

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

/* This function is called from __wrap_main before handing off control to user code. It calls ia2_main which is the user-defined compartment config code */
void ia2_start(void) {
    ia2_set_up_tags();
    verify_tls_padding();
    /* This needs to happen even if there isn't a compartment 0 explicitly specified */
    ia2_stackptr_0[0] = allocate_stack(0);
    ia2_main();
    /* Tell the syscall filter to forbid init-only operations. This mmap() will
    always fail because it maps a non-page-aligned addr with MAP_FIXED, so it
    works as a reasonable signpost no-op. */
    mmap((void *)IA2_FINISH_INIT_MAGIC, 0, 0, MAP_FIXED, -1, 0);
}

static void append_to_name(char *name, int compartment) {
    name[strlen(name) - 1] = '0' + compartment;
}

/* This function is expected to be called from the user-defined ia2_main once per compartment */
void ia2_protect_memory(const char *libs, int compartment, const char *extra_libraries) {
    printf("%s: protecting %s with pkey %d\n", __func__, libs, compartment);

    void *handle = RTLD_DEFAULT;
    const char *dso_sym = "main";

    if (strcmp(libs, "main")) {
        void *handle = dlopen(libs, RTLD_GLOBAL | RTLD_NOW);
        if (!handle) {
            printf("%s: failed to dlopen %s for compartment %d\n", __func__, libs, compartment);
            // TODO: use actual error codes
            exit(-1);
        }
        // TODO: use a symbol that will always be defined. This is specific to tests/two_keys_minimal/plugin.c
        dso_sym = "start_plugin";
    }

    void *dso_addr = dlsym(handle, dso_sym);
    if (!dso_addr) {
        printf("%s: failed to dlsym symbol 'foo' in %s for compartment %d\n", __func__, libs, compartment);
        exit(-2);
    }
    void *dso_shared_start = dlsym(handle, "__start_ia2_shared_data");
    void *dso_shared_stop = dlsym(handle, "__stop_ia2_shared_data");
    if (!dso_shared_stop != !dso_shared_start) {
        // We should not have one be null without the other
        exit(-3);
    }
    // Get the address of things defined by ia2_compartment_init.inc.
    void *dtor_callgate = NULL;
    void *dtor_ptr = NULL;
    void *finalizers = NULL;

    char dtor_name[] = "__wrap_ia2_compartment_destructor_N";
    // Replace 'N' with the ascii character for the compartment number
    append_to_name(dtor_name, compartment);
    dtor_callgate = dlsym(handle, dtor_name);
    assert(dtor_callgate);

    char dtor_ptr_name[] = "compartment_destructor_ptr_N";
    append_to_name(dtor_ptr_name, compartment);
    dtor_ptr = dlsym(handle, dtor_ptr_name);
    assert(dtor_ptr);

    char finalizer_name[] = "finalizers_N";
    append_to_name(finalizer_name, compartment);
    finalizers = dlsym(handle, finalizer_name);
    assert(finalizers);

    if (compartment != 0) {
        void *initial_sp = allocate_stack(compartment);
        char stackptr_name[] = "ia2_stackptr_N";
        append_to_name(stackptr_name, compartment);
        void **stackptr = (void **)dlsym(handle, stackptr_name);
        *stackptr = initial_sp;
    }

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
    ia2_setup_destructors(args.ehdr, compartment, dtor_callgate, dtor_ptr, finalizers);
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
