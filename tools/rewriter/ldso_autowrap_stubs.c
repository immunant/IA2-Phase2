/*
 * Stub translation unit that pulls loader/dl* prototypes from system headers.
 * This file is fed through the rewriter pipeline so FnDecl/DetermineAbi can
 * derive accurate ABI signatures for loader function wrappers.
 *
 * The rewriter will see these declarations and record their signatures,
 * eliminating the need for hand-coded ABI tables.
 */

#define _GNU_SOURCE
#include <dlfcn.h>   /* dlopen/dlmopen/dlclose/dlsym/dlvsym/dladdr/dladdr1/dlinfo/dlerror */
#include <link.h>    /* dl_iterate_phdr */

/* _dl_debug_state is not in public headers; declare manually */
extern void _dl_debug_state(void);
