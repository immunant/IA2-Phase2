#ifndef IA2_LOADER_H
#define IA2_LOADER_H

#ifdef __cplusplus
#include <atomic>
extern "C" {
#else
#include <stdbool.h>
#include <stdatomic.h>
#endif

// Loader compartment (currently shares compartment 1 with main/exit)
static const int ia2_loader_compartment = 1;

// Thread-local flag indicating we're inside a loader gate. The mmap/mremap
// wrappers use it to retag anonymous mappings because the loader and the main
// compartment both run with pkey 1, so observing PKRU alone cannot reveal
// whether an mmap comes from loader code. On AArch64 the gate does not retag
// x18 when it enters, so the allocator shim consults this flag before reading
// the compartment tag from x18. x86-64 PartitionAlloc now infers loader
// context directly from PKRU, but we keep this flag for subsystems and
// architectures where hardware state does not distinguish loader vs. caller.
#ifdef __cplusplus
extern thread_local bool ia2_in_loader_gate;
#else
extern _Thread_local bool ia2_in_loader_gate;
#endif

// Counter of allocations served by loader PartitionAlloc path, exported so the
// allocator shim can prove loader traffic used the dedicated heap.
#ifdef __cplusplus
extern std::atomic<unsigned long> ia2_loader_alloc_count;
#else
extern _Atomic unsigned long ia2_loader_alloc_count;
#endif

// Counter of mmap calls tagged with pkey 1 during loader operations; mmap
// wrappers increment this to aid tests that verify loader retagging.
#ifdef __cplusplus
extern std::atomic<unsigned long> ia2_loader_mmap_count;
#else
extern _Atomic unsigned long ia2_loader_mmap_count;
#endif

#ifdef IA2_DEBUG
// Per-wrapper telemetry counters (debug builds only)
#ifdef __cplusplus
extern std::atomic<unsigned long> ia2_dlopen_count;
extern std::atomic<unsigned long> ia2_dlmopen_count;
extern std::atomic<unsigned long> ia2_dlclose_count;
extern std::atomic<unsigned long> ia2_dlsym_count;
extern std::atomic<unsigned long> ia2_dlvsym_count;
extern std::atomic<unsigned long> ia2_dladdr_count;
extern std::atomic<unsigned long> ia2_dladdr1_count;
extern std::atomic<unsigned long> ia2_dlinfo_count;
extern std::atomic<unsigned long> ia2_dlerror_count;
extern std::atomic<unsigned long> ia2_dl_iterate_phdr_count;
#else
extern _Atomic unsigned long ia2_dlopen_count;
extern _Atomic unsigned long ia2_dlmopen_count;
extern _Atomic unsigned long ia2_dlclose_count;
extern _Atomic unsigned long ia2_dlsym_count;
extern _Atomic unsigned long ia2_dlvsym_count;
extern _Atomic unsigned long ia2_dladdr_count;
extern _Atomic unsigned long ia2_dladdr1_count;
extern _Atomic unsigned long ia2_dlinfo_count;
extern _Atomic unsigned long ia2_dlerror_count;
extern _Atomic unsigned long ia2_dl_iterate_phdr_count;
#endif

// Internal callback counters (for glibc→ld.so paths)
unsigned long ia2_get_dl_catch_exception_count(void);
unsigned long ia2_get_dl_catch_error_count(void);

// Per-wrapper telemetry accessors (for bootstrap shim coverage testing)
unsigned long ia2_get_dlopen_count(void);
unsigned long ia2_get_dlmopen_count(void);
unsigned long ia2_get_dlclose_count(void);
unsigned long ia2_get_dlsym_count(void);
unsigned long ia2_get_dlvsym_count(void);
unsigned long ia2_get_dladdr_count(void);
unsigned long ia2_get_dladdr1_count(void);
unsigned long ia2_get_dlinfo_count(void);
unsigned long ia2_get_dlerror_count(void);
unsigned long ia2_get_dl_iterate_phdr_count(void);

// Debug accessors for gate depth (used in testing)
unsigned int ia2_get_loader_gate_depth(void);

// Debug accessors for PKRU-based gates
unsigned int ia2_get_pkru_gate_depth(void);
unsigned long ia2_get_pkru_gate_switch_count(void);
#ifdef __cplusplus
uint32_t ia2_get_current_pkru(void);
#else
#include <stdint.h>
uint32_t ia2_get_current_pkru(void);
#endif

#endif // IA2_DEBUG

// Enter loader gate (swaps PKRU to allow loader access after initialization)
void ia2_loader_gate_enter(void);

// Exit loader gate (restores PKRU after initialization)
void ia2_loader_gate_exit(void);

// Telemetry accessors (always available)
unsigned long ia2_get_loader_alloc_count(void);
unsigned long ia2_get_loader_mmap_count(void);

// Global flag: when true, PKRU gates will actively switch PKRU
// Set to true after initialization completes (see init.c)
#ifdef __cplusplus
extern std::atomic<bool> ia2_pkru_gates_active;
#else
extern _Atomic bool ia2_pkru_gates_active;
#endif

// Global counter: tracks PKRU gate switches for observability
#ifdef __cplusplus
extern std::atomic<unsigned long> ia2_pkru_gate_switch_count;
#else
extern _Atomic unsigned long ia2_pkru_gate_switch_count;
#endif

#ifdef __cplusplus
}
#endif

#endif // IA2_LOADER_H
