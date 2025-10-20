/*
 * Bootstrap Shim: LD_PRELOAD library that intercepts GLIBC_PRIVATE loader calls
 *
 * PROBLEM: glibc uses internal GLIBC_PRIVATE symbols (__libc_dlopen_mode,
 * __libc_dlsym, __libc_dlclose) for dynamic loading operations that occur
 * before main() runs (e.g., NSS modules, iconv gconv modules). These bypass
 * our normal dlopen/dlsym/dlclose wrappers that enforce PKRU gates.
 *
 * SOLUTION: This LD_PRELOAD shim intercepts the GLIBC_PRIVATE symbols and
 * forwards them through our normal loader gate wrappers, ensuring all loader
 * operations (even pre-main) are subject to compartmentalization.
 *
 * USAGE:
 *   LD_PRELOAD=build/runtime/libia2/liblibia2_bootstrap_shim.so ./my_program
 *
 * ARCHITECTURE:
 *   1. This shim exports __libc_dlopen_mode, __libc_dlsym, __libc_dlclose
 *      with GLIBC_PRIVATE versioning (via bootstrap_shim.version)
 *   2. When glibc calls these symbols, our shim intercepts the calls
 *   3. We forward to __wrap_dlopen, __wrap_dlsym, __wrap_dlclose (our wrappers)
 *   4. Our wrappers enter loader gates, switching PKRU to allow loader access
 *   5. Wrappers call the real __real_dlopen, __real_dlsym, __real_dlclose
 *
 * RELATIONSHIP TO EXISTING WRAPPERS:
 *   - dlopen_wrapper.c already provides __wrap_dlopen, __wrap_dlsym, etc.
 *   - Those wrappers handle gate enter/exit and telemetry
 *   - This shim simply redirects GLIBC_PRIVATE calls to those wrappers
 *   - No duplicate gate logic needed here
 *
 * See tests/dl_debug_test/RTLD_GLOBAL_RO_PATCHING_ATTEMPT.md for background
 * on why we use LD_PRELOAD instead of _rtld_global_ro patching.
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
    // Forward to our existing dlopen wrapper, which handles:
    // - Loader gate enter/exit
    // - PKRU switching (when IA2_USE_PKRU_GATES enabled)
    // - Telemetry (ia2_dlopen_count++)
    // - Forwarding to __real_dlopen
    return __wrap_dlopen(filename, mode);
}

/*
 * __libc_dlsym: GLIBC_PRIVATE symbol used by glibc internally
 *
 * This is called after __libc_dlopen_mode to resolve symbols in the
 * dynamically loaded module.
 *
 * We intercept this and forward to our normal dlsym wrapper.
 */
void *__libc_dlsym(void *handle, const char *symbol) {
    // Forward to our existing dlsym wrapper, which handles:
    // - Loader gate enter/exit
    // - PKRU switching (when IA2_USE_PKRU_GATES enabled)
    // - Telemetry (ia2_dlsym_count++)
    // - Forwarding to __real_dlsym
    return __wrap_dlsym(handle, symbol);
}

/*
 * __libc_dlclose: GLIBC_PRIVATE symbol used by glibc internally
 *
 * This is called to close dynamically loaded modules.
 *
 * We intercept this and forward to our normal dlclose wrapper.
 */
int __libc_dlclose(void *handle) {
    // Forward to our existing dlclose wrapper, which handles:
    // - Loader gate enter/exit
    // - PKRU switching (when IA2_USE_PKRU_GATES enabled)
    // - Telemetry (ia2_dlclose_count++)
    // - Forwarding to __real_dlclose
    return __wrap_dlclose(handle);
}
