/*
 * Bootstrap Shim: LD_PRELOAD library that intercepts GLIBC_PRIVATE loader calls
 *
 * Background: glibc uses internal GLIBC_PRIVATE symbols (__libc_dlopen_mode,
 * __libc_dlsym, __libc_dlclose) for dynamic loading operations that occur
 * before main() runs (e.g., NSS modules, iconv gconv modules). These bypass
 * our normal dlopen/dlsym/dlclose wrappers that enforce PKRU gates.
 *
 * SOLUTION: This LD_PRELOAD shim intercepts the GLIBC_PRIVATE symbols and
 * forwards them through our normal loader gate wrappers, ensuring all loader
 * operations (even pre-main) are subject to compartmentalization.
 *
 * Usage:
 *   LD_PRELOAD=build/runtime/libia2/libia2_bootstrap_shim.so ./my_program
 *
 * Method:
 *   1. This shim exports __libc_dlopen_mode, __libc_dlsym, __libc_dlclose
 *      with GLIBC_PRIVATE versioning (via bootstrap_shim.version)
 *   2. When glibc calls these symbols, our shim intercepts the calls
 *   3. We forward to __wrap_dlopen, __wrap_dlsym, __wrap_dlclose (our wrappers)
 *   4. Our wrappers enter loader gates, switching PKRU to allow loader access
 *   5. Wrappers call the real __real_dlopen, __real_dlsym, __real_dlclose
 *
 *
 * We export these GLIBC_PRIVATE hooks via `LD_PRELOAD` because `_rtld_global_ro`
 * is mapped read-only and its layout varies across glibc releases
 *
 * When is the shim needed?
 * - After ia2_start runs, libc and ld.so are retagged to pkey 1. Post-init
 *   helpers such as iconv_open/NSS lookups still call __libc_dlopen_mode to
 *   load gconv/NSS DSOs. Without the shim, those calls bypass __wrap_dlopen
 *   and touch libc while running under the caller’s protection key, which PKRU
 *   blocks.
 * - Calls to __libc_dlopen_mode that happen before main() execute before the
 *   retag, so they are not the risky path; the shim exists to cover the
 *   post-init indirect loader calls.
 */

#define _GNU_SOURCE
#include <dlfcn.h>

// Forward declarations of our existing wrapper functions from dlopen_wrapper.c
// These are the __wrap_* functions that libia2 provides via --wrap linker flags
extern void *__wrap_dlopen(const char *filename, int flags);
extern void *__wrap_dlsym(void *handle, const char *symbol);
extern int __wrap_dlclose(void *handle);

/*
 * __libc_dlopen_mode: GLIBC_PRIVATE symbol used by glibc internally
 *
 * This is called by:
 * - NSS (Name Service Switch) module loading
 * - iconv gconv module loading
 * - Other glibc internal dynamic loading
 *
 * We intercept this and forward to our normal dlopen wrapper.
 */
void *__libc_dlopen_mode(const char *filename, int mode) {
    // Signatures must stay identical to glibc's private entry points; the
    // __wrap_* functions already flip PKRU, so we only need to forward.
    return __wrap_dlopen(filename, mode);
}

void *__libc_dlsym(void *handle, const char *symbol) {
    return __wrap_dlsym(handle, symbol);
}

int __libc_dlclose(void *handle) {
    return __wrap_dlclose(handle);
}
