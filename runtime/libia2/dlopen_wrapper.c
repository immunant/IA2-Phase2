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

// Check if a DSO is a system/loader library that should be retagged to pkey 1
// Returns true for ld.so, libc, libpthread, etc. - false for application libraries
static bool is_loader_dso(const char *dso_name) {
    if (!dso_name || dso_name[0] == '\0') {
        return false;  // Main executable, not a loader DSO
    }

    // Get basename to ignore directory paths
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
    // Skip pseudo-handles - these aren't real DSO handles
    if (!handle || handle == RTLD_DEFAULT || handle == RTLD_NEXT) {
        return;
    }

    // Get the link_map for this handle
    // We need to enter the gate again because dlinfo/dlerror access loader internals
    struct link_map *map = NULL;

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
        // Don't log here to avoid flooding stderr on real workloads with many DSOs
        return;
    }

    // Loader/system DSO - retag writable segments to loader compartment (pkey 1)
    // Note: ia2_tag_link_map currently calls exit(-1) on failure.
    // This is intentional - memory tagging failures are considered fatal
    // as they compromise the security boundary between compartments.
    const char *display_name = dso_name && dso_name[0] ? dso_name : "(main)";
    ia2_log("Automatically retagging loader DSO %s to compartment 1\n", display_name);
    ia2_tag_link_map(map, ia2_loader_compartment);
    ia2_log("Successfully retagged DSO at base 0x%lx to loader compartment\n", map->l_addr);
}

// Wrapped dlopen: enter loader gate, call real dlopen, exit gate, then retag DSO
void *__wrap_dlopen(const char *filename, int flags) {
    ia2_loader_gate_enter();
    #ifdef IA2_DEBUG
    ia2_dlopen_count++;
    #endif
    void *handle = __real_dlopen(filename, flags);
    ia2_loader_gate_exit();

    // Automatically retag loader/system DSOs to compartment 1
    // Application libraries are skipped to preserve their compartment assignments
    ia2_retag_loaded_dso(handle);

    return handle;
}

// Wrapped dlmopen: enter loader gate, call real dlmopen, exit gate, then retag DSO
void *__wrap_dlmopen(Lmid_t lmid, const char *filename, int flags) {
    ia2_loader_gate_enter();
    #ifdef IA2_DEBUG
    ia2_dlmopen_count++;
    #endif
    void *handle = __real_dlmopen(lmid, filename, flags);
    ia2_loader_gate_exit();

    // Automatically retag newly loaded DSO to loader compartment
    ia2_retag_loaded_dso(handle);

    return handle;
}

// Wrapped dlsym: enter loader gate, call real dlsym, exit gate
void *__wrap_dlsym(void *handle, const char *symbol) {
    ia2_loader_gate_enter();
    #ifdef IA2_DEBUG
    ia2_dlsym_count++;
    #endif
    void *result = __real_dlsym(handle, symbol);
    ia2_loader_gate_exit();
    return result;
}

// Wrapped dlvsym: enter loader gate, call real dlvsym, exit gate
void *__wrap_dlvsym(void *handle, const char *symbol, const char *version) {
    ia2_loader_gate_enter();
    #ifdef IA2_DEBUG
    ia2_dlvsym_count++;
    #endif
    void *result = __real_dlvsym(handle, symbol, version);
    ia2_loader_gate_exit();
    return result;
}

// Wrapped dlclose: enter loader gate, call real dlclose, exit gate
int __wrap_dlclose(void *handle) {
    ia2_loader_gate_enter();
    #ifdef IA2_DEBUG
    ia2_dlclose_count++;
    #endif
    int result = __real_dlclose(handle);
    ia2_loader_gate_exit();
    return result;
}

// Wrapped dladdr: enter loader gate, call real dladdr, exit gate
int __wrap_dladdr(const void *addr, Dl_info *info) {
    ia2_loader_gate_enter();
    #ifdef IA2_DEBUG
    ia2_dladdr_count++;
    #endif
    int result = __real_dladdr(addr, info);
    ia2_loader_gate_exit();
    return result;
}

// Wrapped dladdr1: enter loader gate, call real dladdr1, exit gate
int __wrap_dladdr1(const void *addr, Dl_info *info, void **extra_info, int flags) {
    ia2_loader_gate_enter();
    #ifdef IA2_DEBUG
    ia2_dladdr1_count++;
    #endif
    int result = __real_dladdr1(addr, info, extra_info, flags);
    ia2_loader_gate_exit();
    return result;
}

// Wrapped dlinfo: enter loader gate, call real dlinfo, exit gate
int __wrap_dlinfo(void *handle, int request, void *arg) {
    ia2_loader_gate_enter();
    #ifdef IA2_DEBUG
    ia2_dlinfo_count++;
    #endif
    int result = __real_dlinfo(handle, request, arg);
    ia2_loader_gate_exit();
    return result;
}

// Wrapped dlerror: enter loader gate, call real dlerror, exit gate
char *__wrap_dlerror(void) {
    ia2_loader_gate_enter();
    #ifdef IA2_DEBUG
    ia2_dlerror_count++;
    #endif
    char *result = __real_dlerror();
    ia2_loader_gate_exit();
    return result;
}

// Wrapped dl_iterate_phdr: enter loader gate, call real dl_iterate_phdr, exit gate
int __wrap_dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data) {
    ia2_loader_gate_enter();
    #ifdef IA2_DEBUG
    ia2_dl_iterate_phdr_count++;
    #endif
    int result = __real_dl_iterate_phdr(callback, data);
    ia2_loader_gate_exit();
    return result;
}

