#define _GNU_SOURCE
#include <dlfcn.h>
#include <ia2_loader.h>
#include <ia2.h>

// Forward declarations for real symbols
extern void *__real_dlopen(const char *filename, int flags);
extern void *__real_dlmopen(Lmid_t lmid, const char *filename, int flags);
extern void *__real_dlsym(void *handle, const char *symbol);
extern void *__real_dlvsym(void *handle, const char *symbol, const char *version);

// Wrapped dlopen: enter loader gate, call real dlopen, exit gate
// Phase 1: Just set gate flag for allocator routing
// Phase 2: Will add PKRU swap and DSO retagging
void *__wrap_dlopen(const char *filename, int flags) {
    ia2_loader_gate_enter();
    void *handle = __real_dlopen(filename, flags);
    ia2_loader_gate_exit();

    // TODO Phase 1.5: Add DSO retagging after testing allocator path
    // if (handle && handle != RTLD_DEFAULT && handle != RTLD_NEXT) {
    //     struct link_map *map = NULL;
    //     if (dlinfo(handle, RTLD_DI_LINKMAP, &map) == 0 && map) {
    //         ia2_tag_link_map(map, ia2_loader_compartment);
    //     }
    // }

    return handle;
}

// Wrapped dlmopen: enter loader gate, call real dlmopen, exit gate
// Phase 1: Just set gate flag for allocator routing
void *__wrap_dlmopen(Lmid_t lmid, const char *filename, int flags) {
    ia2_loader_gate_enter();
    void *handle = __real_dlmopen(lmid, filename, flags);
    ia2_loader_gate_exit();

    // TODO Phase 1.5: Add DSO retagging
    return handle;
}

// Wrapped dlsym: enter loader gate, call real dlsym, exit gate
void *__wrap_dlsym(void *handle, const char *symbol) {
    ia2_loader_gate_enter();
    void *result = __real_dlsym(handle, symbol);
    ia2_loader_gate_exit();
    return result;
}

// Wrapped dlvsym: enter loader gate, call real dlvsym, exit gate
void *__wrap_dlvsym(void *handle, const char *symbol, const char *version) {
    ia2_loader_gate_enter();
    void *result = __real_dlvsym(handle, symbol, version);
    ia2_loader_gate_exit();
    return result;
}
