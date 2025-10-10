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

// Thread-local flag indicating we're inside a loader gate
#ifdef __cplusplus
extern thread_local bool ia2_in_loader_gate;
#else
extern _Thread_local bool ia2_in_loader_gate;
#endif

// Counter of allocations served by loader PartitionAlloc path
#ifdef __cplusplus
extern std::atomic<unsigned long> ia2_loader_alloc_count;
#else
extern _Atomic unsigned long ia2_loader_alloc_count;
#endif

// Counter of mmap calls tagged with pkey 1 during loader operations
#ifdef __cplusplus
extern std::atomic<unsigned long> ia2_loader_mmap_count;
#else
extern _Atomic unsigned long ia2_loader_mmap_count;
#endif

// Enter loader gate (future: swap PKRU to allow loader access)
void ia2_loader_gate_enter(void);

// Exit loader gate (future: restore PKRU)
void ia2_loader_gate_exit(void);

#ifdef __cplusplus
}
#endif

#endif // IA2_LOADER_H
