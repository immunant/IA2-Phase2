#pragma once

#include "ia2_memory_maps.h"
#include "ia2_internal.h"

// Only enable this code that stores these addresses when debug logging is enabled.
// This reduces the trusted codebase and avoids runtime overhead.
#if IA2_DEBUG_MEMORY

// Moved `ia2_thread_metadata` from here to `ia2_internal.h`
// so that it can be used in `ia2_internal.h` within `_IA2_INIT_RUNTIME`
// to only initialize the `ia2_threads_metadata` global once.

/// Allocate and initialize a new `ia2_thread_metadata` for the current thread.
/// Importantly, this may only be called once per thread.
///
/// The returned pointer is stable, non-`NULL`,
/// and will not be moved or deallocated/uninitialized.
/// Operations on the `ia2_thread_metadata` must be atomic.
///
/// If too many threads are created, this will `abort`
/// and `IA2_MAX_THREADS` can be increased.
struct ia2_thread_metadata *ia2_thread_metadata_new_for_current_thread(void);

/// Find the `ia2_thread_metadata` for the current thread.
///
/// `ia2_thread_metadata_new_for_current_thread`
/// must have been previously called for this thread,
/// or else the `ia2_thread_metadata` will not be found and this will `abort`.
///
/// The returned pointer is stable, non-`NULL`,
/// and will not be moved or deallocated/uninitialized.
/// Operations on the `ia2_thread_metadata` must be atomic.
struct ia2_thread_metadata *ia2_thread_metadata_get_for_current_thread(void);

struct ia2_addr_location {
  /// A descriptive name of what this address points to.
  /// For example, "stack".
  ///
  /// `NULL` if unknown.
  const char *name;

  /// The metadata of the thread this address belongs to.
  ///
  /// Fields must be read atomically.
  const struct ia2_thread_metadata *thread_metadata;

  /// The compartment this address is in.
  ///
  /// `-1` if unknown.
  int compartment;
};

/// Find the `ia2_addr_location` of `addr`.
/// If it is not found or there is an error,
/// the fields are set to `NULL` or `-1` depending on the type.
struct ia2_addr_location ia2_addr_location_find(uintptr_t addr);

#endif // IA2_DEBUG_MEMORY
