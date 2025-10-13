#define _GNU_SOURCE
#include <dlfcn.h>
#include <ia2_loader.h>
#include <ia2.h>

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


// Wrapped dlopen: enter loader gate, call real dlopen, exit gate
// Phase 1: Just set gate flag for allocator routing
// Phase 2: Will add PKRU swap and DSO retagging
void *__wrap_dlopen(const char *filename, int flags) {
    ia2_loader_gate_enter();
    #ifdef IA2_DEBUG
    ia2_dlopen_count++;
    #endif
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
    #ifdef IA2_DEBUG
    ia2_dlmopen_count++;
    #endif
    void *handle = __real_dlmopen(lmid, filename, flags);
    ia2_loader_gate_exit();

    // TODO Phase 1.5: Add DSO retagging
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

