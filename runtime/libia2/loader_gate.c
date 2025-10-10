#include <ia2_loader.h>
#include <stdatomic.h>

// Thread-local flag: true when inside loader gate
_Thread_local bool ia2_in_loader_gate = false;

// Global counter: tracks allocations served via loader PartitionAlloc path
_Atomic unsigned long ia2_loader_alloc_count = 0;

// Global counter: tracks mmap calls tagged with pkey 1 during loader operations
_Atomic unsigned long ia2_loader_mmap_count = 0;

// Enter loader gate
// Phase 1: Just set flag (PKRU swap will be added in Phase 2)
void ia2_loader_gate_enter(void) {
    ia2_in_loader_gate = true;
}

// Exit loader gate
// Phase 1: Just clear flag (PKRU restore will be added in Phase 2)
void ia2_loader_gate_exit(void) {
    ia2_in_loader_gate = false;
}
