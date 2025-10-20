#include <ia2_loader.h>
#include <ia2_internal.h>
#include <stdatomic.h>
#include <stdint.h>

#ifdef IA2_USE_PKRU_GATES
// Forward declaration of cache invalidation function from partition-alloc
extern void ia2_invalidate_pkru_cache(void);
#endif

// Thread-local flag: true when inside loader gate
_Thread_local bool ia2_in_loader_gate = false;

// Thread-local recursion counter: tracks nested loader calls
_Thread_local unsigned int ia2_loader_gate_depth = 0;

#ifdef IA2_USE_PKRU_GATES
// Maximum nesting depth for loader gates
#define IA2_MAX_GATE_DEPTH 32

// Thread-local PKRU save stack for nested gates
_Thread_local uint32_t ia2_saved_pkru[IA2_MAX_GATE_DEPTH] = {0};

// Thread-local PKRU gate depth (separate from flag-based depth for now)
_Thread_local unsigned int ia2_pkru_gate_depth = 0;

// Global flag: defer PKRU switching until initialization completes
// During early initialization, memory isn't tagged yet, so PKRU switching would break
_Atomic bool ia2_pkru_gates_active = false;
#endif // IA2_USE_PKRU_GATES

// Global counter: tracks allocations served via loader PartitionAlloc path
_Atomic unsigned long ia2_loader_alloc_count = 0;

// Global counter: tracks mmap calls tagged with pkey 1 during loader operations
_Atomic unsigned long ia2_loader_mmap_count = 0;

#ifdef IA2_USE_PKRU_GATES
// Global counter: tracks PKRU gate switches (for observability)
_Atomic unsigned long ia2_pkru_gate_switch_count = 0;
#endif

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
// When IA2_USE_PKRU_GATES is enabled, also saves PKRU and switches to loader PKRU
void ia2_loader_gate_enter(void) {
    // Flag-based gate (always active for compatibility)
    if (ia2_loader_gate_depth == 0) {
        ia2_in_loader_gate = true;
    }
    ia2_loader_gate_depth++;

#ifdef IA2_USE_PKRU_GATES
    // PKRU-based gate (hardware-enforced, but only after initialization)
    if (ia2_pkru_gates_active && ia2_pkru_gate_depth < IA2_MAX_GATE_DEPTH) {
        // Save current PKRU
        ia2_saved_pkru[ia2_pkru_gate_depth] = ia2_read_pkru();
        ia2_pkru_gate_depth++;

        // Switch to loader PKRU:
        // - Allow pkey 0 (shared data)
        // - Allow pkey 1 (loader compartment)
        // - Block all other pkeys (compartments 2-15)
        // PKRU encoding: 2 bits per pkey, 00 = allow, 11 = deny
        // 0xfffffff0 = ...1111_1111_1111_0000 (allow pkeys 0 and 1, block rest)
        ia2_write_pkru(0xfffffff0);

        // Invalidate PKRU cache in allocator
        ia2_invalidate_pkru_cache();

        // Increment telemetry
        ia2_pkru_gate_switch_count++;
    }
#endif
}

// Exit loader gate
// Only clears flag when returning from outermost loader call
// When IA2_USE_PKRU_GATES is enabled, also restores saved PKRU
void ia2_loader_gate_exit(void) {
    // Flag-based gate (always active for compatibility)
    if (ia2_loader_gate_depth > 0) {
        ia2_loader_gate_depth--;
        if (ia2_loader_gate_depth == 0) {
            ia2_in_loader_gate = false;
        }
    }

#ifdef IA2_USE_PKRU_GATES
    // PKRU-based gate (hardware-enforced, but only after initialization)
    if (ia2_pkru_gates_active && ia2_pkru_gate_depth > 0) {
        ia2_pkru_gate_depth--;
        // Restore saved PKRU
        ia2_write_pkru(ia2_saved_pkru[ia2_pkru_gate_depth]);

        // Invalidate PKRU cache in allocator
        ia2_invalidate_pkru_cache();

        // Increment telemetry
        ia2_pkru_gate_switch_count++;
    }
#endif
}

// Telemetry accessors (always available)
unsigned long ia2_get_loader_alloc_count(void) {
    return atomic_load(&ia2_loader_alloc_count);
}

unsigned long ia2_get_loader_mmap_count(void) {
    return atomic_load(&ia2_loader_mmap_count);
}

#ifdef IA2_DEBUG
// Debug accessor to query current gate depth
// Used for testing nested gate behavior
unsigned int ia2_get_loader_gate_depth(void) {
    return ia2_loader_gate_depth;
}

// Per-wrapper telemetry accessors (for bootstrap shim coverage testing)
unsigned long ia2_get_dlopen_count(void) {
    return atomic_load(&ia2_dlopen_count);
}

unsigned long ia2_get_dlmopen_count(void) {
    return atomic_load(&ia2_dlmopen_count);
}

unsigned long ia2_get_dlclose_count(void) {
    return atomic_load(&ia2_dlclose_count);
}

unsigned long ia2_get_dlsym_count(void) {
    return atomic_load(&ia2_dlsym_count);
}

unsigned long ia2_get_dlvsym_count(void) {
    return atomic_load(&ia2_dlvsym_count);
}

unsigned long ia2_get_dladdr_count(void) {
    return atomic_load(&ia2_dladdr_count);
}

unsigned long ia2_get_dladdr1_count(void) {
    return atomic_load(&ia2_dladdr1_count);
}

unsigned long ia2_get_dlinfo_count(void) {
    return atomic_load(&ia2_dlinfo_count);
}

unsigned long ia2_get_dlerror_count(void) {
    return atomic_load(&ia2_dlerror_count);
}

unsigned long ia2_get_dl_iterate_phdr_count(void) {
    return atomic_load(&ia2_dl_iterate_phdr_count);
}

#ifdef IA2_USE_PKRU_GATES
// Debug accessor to query current PKRU gate depth
unsigned int ia2_get_pkru_gate_depth(void) {
    return ia2_pkru_gate_depth;
}

// Debug accessor for PKRU gate switch count
unsigned long ia2_get_pkru_gate_switch_count(void) {
    return atomic_load(&ia2_pkru_gate_switch_count);
}

// Debug accessor to read current PKRU value
uint32_t ia2_get_current_pkru(void) {
    return ia2_read_pkru();
}
#endif
#endif // IA2_DEBUG
