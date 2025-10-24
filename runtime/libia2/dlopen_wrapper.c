#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>  // For struct dl_phdr_info
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <ia2_loader.h>
#include <ia2.h>
#include <ia2_internal.h>  // For ia2_log

// Forward declarations for real symbols
extern void *__real_dlopen(const char *filename, int flags);
extern void *__real_dlmopen(Lmid_t lmid, const char *filename, int flags);
extern void *__real_dlsym(void *handle, const char *symbol);
extern void *__real_dlvsym(void *handle, const char *symbol, const char *version);
extern int __real_dlclose(void *handle);
extern int __real_dladdr(const void *addr, Dl_info *info);
extern int __real_dladdr1(const void *addr, Dl_info *info, void **extra_info, int flags);
extern int __real_dlinfo(void *handle, int request, void *arg);
extern char *__real_dlerror(void);
extern int __real_dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data);


// Check if a DSO is a system/loader library that should be retagged to pkey 1.
// The prefixes below cover glibc-owned DSOs whose writable regions maintain
// loader metadata. If glibc adds new loader helpers, extend this allowlist so
// they continue to land on the loader compartment.
// Returns true for ld.so, libc, libpthread, etc. - false for application libraries
static bool is_loader_dso(const char *dso_name) {
    if (!dso_name || dso_name[0] == '\0') {
        return false;  // Main executable, not a loader DSO
    }

    const char *basename = strrchr(dso_name, '/');
    basename = basename ? (basename + 1) : dso_name;

    // Check for known loader/system libraries
    // These should be retagged to compartment 1 (loader compartment)
    if (strncmp(basename, "ld-linux", 8) == 0) return true;        // ld-linux-x86-64.so.2, ld-linux-aarch64.so.1
    if (strncmp(basename, "libc.so", 7) == 0) return true;         // libc.so.6
    if (strncmp(basename, "libc-", 5) == 0) return true;           // libc-2.35.so
    if (strncmp(basename, "libpthread", 10) == 0) return true;     // libpthread.so.0
    if (strncmp(basename, "libdl.so", 8) == 0) return true;        // libdl.so.2
    if (strncmp(basename, "librt.so", 8) == 0) return true;        // librt.so.1
    if (strncmp(basename, "libm.so", 7) == 0) return true;         // libm.so.6

    // All other DSOs are application libraries - keep their original compartment
    return false;
}

// Helper function to retag loader/system DSOs to the loader compartment (pkey 1)
// Application DSOs are skipped to preserve their compartment assignments
// Called after successful dlopen/dlmopen to ensure loader-owned memory uses pkey 1
// Note: Must be called AFTER ia2_loader_gate_exit() to avoid recursion
static void ia2_retag_loaded_dso(void *handle) {
    // Skip pseudo-handles
    if (!handle || handle == RTLD_DEFAULT || handle == RTLD_NEXT) {
        return;
    }

    // Get the link_map for this handle
    struct link_map *map = NULL;

    // Re-enter gate for dlinfo/dlerror (they access loader internals like link_map chain).
    // Gate was exited after dlopen to avoid recursion during retagging; depth counter
    // makes this re-entry safe. Same pattern below for dlerror if dlinfo fails.
    ia2_loader_gate_enter();
    int dlinfo_result = __real_dlinfo(handle, RTLD_DI_LINKMAP, &map);
    ia2_loader_gate_exit();

    if (dlinfo_result != 0) {
        // dlinfo failed - log but don't crash
        ia2_loader_gate_enter();
        char *error = __real_dlerror();
        ia2_loader_gate_exit();
        ia2_log("Warning: dlinfo failed for handle %p: %s\n", handle, error ? error : "unknown error");
        return;
    }

    if (!map) {
        ia2_log("Warning: dlinfo returned NULL link_map for handle %p\n", handle);
        return;
    }

    // Check if this is a loader/system DSO or an application library
    const char *dso_name = map->l_name;
    if (!is_loader_dso(dso_name)) {
        // Application library - skip retagging to preserve compartment assignment
        return;
    }

    const char *display_name = (dso_name && dso_name[0]) ? dso_name : "(main)";

    // Honor explicit compartment assignments: if the runtime registered this
    // system library for some other compartment, leave the mapping alone.
    int assigned_compartment = ia2_lookup_registered_compartment(dso_name);
    if (assigned_compartment > 0 && assigned_compartment != ia2_loader_compartment) {
        ia2_log("Skipping loader retag for %s; registered to compartment %d\n",
                display_name, assigned_compartment);
        return;
    }

    // Loader/system DSO - retag writable segments to loader compartment (pkey 1)
    // Note: ia2_tag_link_map currently calls exit(-1) on failure.
    ia2_log("Automatically retagging loader DSO %s to compartment 1\n", display_name);
    ia2_tag_link_map(map, ia2_loader_compartment);
    ia2_log("Successfully retagged DSO at base 0x%lx to loader compartment\n", map->l_addr);
}

// Wrapped dlopen: enter gate, call real dlopen, exit gate, then retag
//
// KNOWN ISSUE (constructor privilege escalation - tracked for future fix):
// - ELF constructors run before __real_dlopen returns, so the loader gate has not
//   been dropped yet.
// - Constructors observe ia2_in_loader_gate = true and the active PKRU, so their
//   allocations route to pkey 1 via ia2_get_pkey()
// - The IA2 rewriter now emits a priority-101 constructor that exits the gate
//   before most IA2-instrumented constructors run, but third-party DSOs (or
//   IA2 modules with constructors <101) can still execute while the gate is active
// - Those constructors can therefore read or write loader-protected memory until
//   we find a hook that toggles the gate prior to user constructors
void *__wrap_dlopen(const char *filename, int flags) {
    ia2_loader_gate_enter();
    void *handle = __real_dlopen(filename, flags);
    ia2_loader_gate_exit();

    #ifdef IA2_DEBUG
    ia2_dlopen_count++;
    #endif

    // Retag loader/system DSOs to compartment 1
    ia2_retag_loaded_dso(handle);

    return handle;
}

// Wrapped dlmopen: enter gate, call real dlmopen, exit gate, then retag
//
// NOTE: Same tradeoff as dlopen - gate active to preserve loader isolation,
// with constructor privilege escalation as a known issue to be addressed in future work.
void *__wrap_dlmopen(Lmid_t lmid, const char *filename, int flags) {
    ia2_loader_gate_enter();
    void *handle = __real_dlmopen(lmid, filename, flags);
    ia2_loader_gate_exit();

    #ifdef IA2_DEBUG
    ia2_dlmopen_count++;
    #endif

    // Retag loader/system DSOs to compartment 1
    ia2_retag_loaded_dso(handle);

    return handle;
}

void *__wrap_dlsym(void *handle, const char *symbol) {
    ia2_loader_gate_enter();
    void *result = __real_dlsym(handle, symbol);
    ia2_loader_gate_exit();
    #ifdef IA2_DEBUG
    ia2_dlsym_count++;
    #endif
    return result;
}

void *__wrap_dlvsym(void *handle, const char *symbol, const char *version) {
    ia2_loader_gate_enter();
    void *result = __real_dlvsym(handle, symbol, version);
    ia2_loader_gate_exit();
    #ifdef IA2_DEBUG
    ia2_dlvsym_count++;
    #endif
    return result;
}

int __wrap_dlclose(void *handle) {
    ia2_loader_gate_enter();
    int result = __real_dlclose(handle);
    ia2_loader_gate_exit();
    #ifdef IA2_DEBUG
    ia2_dlclose_count++;
    #endif
    return result;
}

int __wrap_dladdr(const void *addr, Dl_info *info) {
    ia2_loader_gate_enter();
    int result = __real_dladdr(addr, info);
    ia2_loader_gate_exit();
    #ifdef IA2_DEBUG
    ia2_dladdr_count++;
    #endif
    return result;
}

int __wrap_dladdr1(const void *addr, Dl_info *info, void **extra_info, int flags) {
    ia2_loader_gate_enter();
    int result = __real_dladdr1(addr, info, extra_info, flags);
    ia2_loader_gate_exit();
    #ifdef IA2_DEBUG
    ia2_dladdr1_count++;
    #endif
    return result;
}

int __wrap_dlinfo(void *handle, int request, void *arg) {
    ia2_loader_gate_enter();
    int result = __real_dlinfo(handle, request, arg);
    ia2_loader_gate_exit();
    #ifdef IA2_DEBUG
    ia2_dlinfo_count++;
    #endif
    return result;
}

char *__wrap_dlerror(void) {
    ia2_loader_gate_enter();
    char *result = __real_dlerror();
    ia2_loader_gate_exit();
    #ifdef IA2_DEBUG
    ia2_dlerror_count++;
    #endif
    return result;
}

struct dl_iterate_callback_wrapper_state {
    int (*user_callback)(struct dl_phdr_info *info, size_t size, void *data);
    void *user_data;
};

// Trampoline callback that exits the loader gate, invokes user callback, then re-enters
static int dl_iterate_callback_trampoline(struct dl_phdr_info *info, size_t size, void *data) {
    struct dl_iterate_callback_wrapper_state *state = (struct dl_iterate_callback_wrapper_state *)data;

    // Exit loader gate before invoking user callback
    // This ensures user code runs with its original PKRU, not loader privileges
    ia2_loader_gate_exit();

    // Invoke the user's callback with their data
    int result = state->user_callback(info, size, state->user_data);

    // Re-enter loader gate for next iteration or return to wrapper
    ia2_loader_gate_enter();

    return result;
}

// Wrapped dl_iterate_phdr: interpose on callbacks
//
// NOTE: User callbacks must NOT run with ia2_in_loader_gate=true, but rather
// use their original compartment privileges.
//
// Solution: Wrap the callback with a trampoline that temporarily exits the gate
int __wrap_dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data) {
    ia2_loader_gate_enter();

    // Package user callback and data into wrapper state
    struct dl_iterate_callback_wrapper_state wrapper_state = {
        .user_callback = callback,
        .user_data = data
    };

    // Call real dl_iterate_phdr with our trampoline, which will exit/enter gate around user callback
    int result = __real_dl_iterate_phdr(dl_iterate_callback_trampoline, &wrapper_state);

    ia2_loader_gate_exit();
    #ifdef IA2_DEBUG
    ia2_dl_iterate_phdr_count++;
    #endif
    return result;
}
