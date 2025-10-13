#include <ia2_loader.h>
#include <stdatomic.h>

// Thread-local flag: true when inside loader gate
_Thread_local bool ia2_in_loader_gate = false;

// Thread-local recursion counter: tracks nested loader calls
_Thread_local unsigned int ia2_loader_gate_depth = 0;

// Global counter: tracks allocations served via loader PartitionAlloc path
_Atomic unsigned long ia2_loader_alloc_count = 0;

// Global counter: tracks mmap calls tagged with pkey 1 during loader operations
_Atomic unsigned long ia2_loader_mmap_count = 0;

#ifdef IA2_DEBUG
// Per-wrapper telemetry counters (debug builds only)
_Atomic unsigned long ia2_dlopen_count = 0;
_Atomic unsigned long ia2_dlmopen_count = 0;
_Atomic unsigned long ia2_dlclose_count = 0;
_Atomic unsigned long ia2_dlsym_count = 0;
_Atomic unsigned long ia2_dlvsym_count = 0;
_Atomic unsigned long ia2_dladdr_count = 0;
_Atomic unsigned long ia2_dladdr1_count = 0;
_Atomic unsigned long ia2_dlinfo_count = 0;
_Atomic unsigned long ia2_dlerror_count = 0;
_Atomic unsigned long ia2_dl_iterate_phdr_count = 0;
#endif

// Enter loader gate
// Uses recursion counter to handle nested loader calls correctly
void ia2_loader_gate_enter(void) {
    if (ia2_loader_gate_depth == 0) {
        ia2_in_loader_gate = true;
    }
    ia2_loader_gate_depth++;
}

// Exit loader gate
// Only clears flag when returning from outermost loader call
void ia2_loader_gate_exit(void) {
    if (ia2_loader_gate_depth > 0) {
        ia2_loader_gate_depth--;
        if (ia2_loader_gate_depth == 0) {
            ia2_in_loader_gate = false;
        }
    }
}
