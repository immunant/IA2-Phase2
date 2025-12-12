// Library B - compartment 3
#include <stdbool.h>
#include <string.h>
#include <dlfcn.h>
#include <ia2.h>

#define IA2_COMPARTMENT 3
#include <ia2_compartment_init.inc>

void lib_b_noop(void) {
    // Empty function for cross-compartment call testing
}

// Shared Dl_info so loader compartment can write to it
IA2_SHARED_DATA static Dl_info shared_info;

// Call loader entry points from compartment 3 to exercise autowrapped
// dlsym/dladdr callgates from a second non-libc compartment.
bool lib_b_verify_loader_wrappers(void) {
    dlerror();  // Clear any prior error state

    void *lib_a_noop_fn = dlsym(RTLD_DEFAULT, "lib_a_noop");
    if (!lib_a_noop_fn) {
        return false;
    }

    // Use shared Dl_info so loader compartment can write to it
    memset(&shared_info, 0, sizeof(shared_info));
    if (dladdr(lib_a_noop_fn, &shared_info) == 0) {
        return false;
    }

    // dli_fname may include full paths or loader-specific names; dli_sname is the
    // stable symbol name reported by dladdr(3), so prefer it for verification.
    if (!shared_info.dli_sname || strcmp(shared_info.dli_sname, "lib_a_noop") != 0) {
        return false;
    }

    return true;
}
