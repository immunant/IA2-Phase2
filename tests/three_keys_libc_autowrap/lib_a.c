// Library A - compartment 2
#include <stdbool.h>
#include <string.h>
#include <dlfcn.h>
#include <ia2.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

void lib_a_noop(void) {
    // Empty function for cross-compartment call testing
}

// Shared Dl_info so loader compartment can write to it
IA2_SHARED_DATA static Dl_info shared_info;

// Call loader entry points from compartment 2 to ensure the autowrapped
// callgates for dlsym/dladdr are usable outside the libc/loader compartment.
bool lib_a_verify_loader_wrappers(void) {
    dlerror();  // Clear any prior error state

    void *lib_b_noop_fn = dlsym(RTLD_DEFAULT, "lib_b_noop");
    if (!lib_b_noop_fn) {
        return false;
    }

    // Use shared Dl_info so loader compartment can write to it
    memset(&shared_info, 0, sizeof(shared_info));
    if (dladdr(lib_b_noop_fn, &shared_info) == 0) {
        return false;
    }

    // dli_fname may include full paths or loader-specific names; dli_sname is the
    // stable symbol name reported by dladdr(3), so prefer it for verification.
    if (!shared_info.dli_sname || strcmp(shared_info.dli_sname, "lib_b_noop") != 0) {
        return false;
    }

    return true;
}
