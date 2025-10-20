#include "get_pkey.h"
#include <ia2_loader.h>

// These functions are duplicated here from libia2 so that we don't have to link
// the allocator against libia2 itself. We want users with libia2 disabled to
// still be able to call `shared_malloc` etc.

#ifdef IA2_USE_PKRU_GATES
// TLS cache for PKRU value to avoid repeated rdpkru instructions
// Cache is invalidated whenever PKRU changes (gate enter/exit, compartment transitions)
thread_local uint32_t ia2_cached_pkru = 0;
thread_local bool ia2_pkru_dirty = true;  // Start dirty to force initial read

// Invalidate the PKRU cache - must be called after any PKRU write
// Exported with default visibility so libia2 can call it
extern "C" __attribute__((__visibility__("default"))) void ia2_invalidate_pkru_cache() {
  ia2_pkru_dirty = true;
}
#endif

#ifdef __x86_64__
__attribute__((__visibility__("hidden")))
uint32_t
ia2_get_pkru() {
  uint32_t pkru = 0;
  __asm__ volatile("rdpkru" : "=a"(pkru) : "a"(0), "d"(0), "c"(0));
  return pkru;
}

#ifdef IA2_USE_PKRU_GATES
// Decode pkey from PKRU value using cached value when possible
// PKRU encoding: 2 bits per pkey, 00 = allow, 11 = deny
// We identify the current compartment as the single allowed non-zero pkey
__attribute__((__visibility__("hidden")))
size_t
ia2_get_pkey_from_pkru() {
  // Use cached value if valid
  uint32_t pkru;
  if (ia2_pkru_dirty) {
    __asm__ volatile("rdpkru" : "=a"(pkru) : "a"(0), "d"(0), "c"(0));
    ia2_cached_pkru = pkru;
    ia2_pkru_dirty = false;
  } else {
    pkru = ia2_cached_pkru;
  }

  // Decode PKRU to find the active compartment pkey
  // PKRU has 2 bits per pkey: bits [1:0] for pkey 0, [3:2] for pkey 1, etc.
  // Pattern: 0xfffffffc = only pkey 0 accessible, 0xfffffff0 = pkeys 0+1 accessible
  switch (pkru) {
  case 0xFFFFFFFC: {
    return 0;
  }
  case 0xFFFFFFF0: {
    return 1;
  }
  case 0xFFFFFFCC: {
    return 2;
  }
  case 0xFFFFFF3C: {
    return 3;
  }
  case 0xFFFFFCFC: {
    return 4;
  }
  case 0xFFFFF3FC: {
    return 5;
  }
  case 0xFFFFCFFC: {
    return 6;
  }
  case 0xFFFF3FFC: {
    return 7;
  }
  case 0xFFFCFFFC: {
    return 8;
  }
  case 0xFFF3FFFC: {
    return 9;
  }
  case 0xFFCFFFFC: {
    return 10;
  }
  case 0xFF3FFFFC: {
    return 11;
  }
  case 0xFCFFFFFC: {
    return 12;
  }
  case 0xF3FFFFFC: {
    return 13;
  }
  case 0xCFFFFFFC: {
    return 14;
  }
  case 0x3FFFFFFC: {
    return 15;
  }
  // TODO: We currently treat any unexpected PKRU value as pkey 0 (the shared
  // heap) for simplicity since glibc(?) initializes the PKRU to 0x55555554
  // (usually). We don't set the PKRU until the first compartment transition, so
  // let's default to using the shared heap before our first wrpkru. When we
  // initialize the PKRU properly (see issue #95) we should probably abort when
  // we see unexpected PKRU values.
  default: {
    return 0;
  }
  }
}

// Phase 2: PKRU-aware allocator - reads PKRU as source of truth
__attribute__((__visibility__("hidden")))
size_t
ia2_get_pkey() {
  // Decode pkey from current PKRU (uses cached value when available)
  size_t pkey = ia2_get_pkey_from_pkru();

  // If we're in loader context (pkey 1), increment telemetry
  if (pkey == 1) {
    ia2_loader_alloc_count.fetch_add(1, std::memory_order_relaxed);
  }

  return pkey;
}
#else
// Phase 1: Flag-based allocator (fallback when PKRU gates disabled)
__attribute__((__visibility__("hidden")))
size_t
ia2_get_pkey() {
  // Phase 1: Check if we're in loader gate - if so, force pkey 1
  // This routes loader allocations to compartment 1 PartitionAlloc
  if (ia2_in_loader_gate) {
    ia2_loader_alloc_count.fetch_add(1, std::memory_order_relaxed);
    return 1;
  }

  uint32_t pkru;
  __asm__("rdpkru" : "=a"(pkru) : "a"(0), "d"(0), "c"(0));
  switch (pkru) {
  case 0xFFFFFFFC: {
    return 0;
  }
  case 0xFFFFFFF0: {
    return 1;
  }
  case 0xFFFFFFCC: {
    return 2;
  }
  case 0xFFFFFF3C: {
    return 3;
  }
  case 0xFFFFFCFC: {
    return 4;
  }
  case 0xFFFFF3FC: {
    return 5;
  }
  case 0xFFFFCFFC: {
    return 6;
  }
  case 0xFFFF3FFC: {
    return 7;
  }
  case 0xFFFCFFFC: {
    return 8;
  }
  case 0xFFF3FFFC: {
    return 9;
  }
  case 0xFFCFFFFC: {
    return 10;
  }
  case 0xFF3FFFFC: {
    return 11;
  }
  case 0xFCFFFFFC: {
    return 12;
  }
  case 0xF3FFFFFC: {
    return 13;
  }
  case 0xCFFFFFFC: {
    return 14;
  }
  case 0x3FFFFFFC: {
    return 15;
  }
  // TODO: We currently treat any unexpected PKRU value as pkey 0 (the shared
  // heap) for simplicity since glibc(?) initializes the PKRU to 0x55555554
  // (usually). We don't set the PKRU until the first compartment transition, so
  // let's default to using the shared heap before our first wrpkru. When we
  // initialize the PKRU properly (see issue #95) we should probably abort when
  // we see unexpected PKRU values.
  default: {
    return 0;
  }
  }
}
#endif
#endif

#ifdef __aarch64__
__attribute__((__visibility__("hidden")))
size_t
ia2_get_pkey() {
  // Phase 1: Check if we're in loader gate - if so, force pkey 1
  if (ia2_in_loader_gate) {
    ia2_loader_alloc_count.fetch_add(1, std::memory_order_relaxed);
    return 1;
  }

  size_t x18;
  asm("mov %0, x18" : "=r"(x18));
  return x18 >> 56;
}
#endif
