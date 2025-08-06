#pragma once

#include "ia2_memory_maps.h"
#include "ia2_internal.h"

// Only enable this code that stores these addresses when debug logging is enabled.
// This reduces the trusted codebase and avoids runtime overhead.
#if IA2_DEBUG_MEMORY

// Moved `ia2_thread_metadata` from here to `ia2_internal.h`
// so that it can be used in `ia2_internal.h` within `_IA2_INIT_RUNTIME`
// to only initialize the `ia2_threads_metadata` global once.

/// Find the `struct ia2_thread_metadata*` for the current thread,
/// adding (but not allocating) one if there isn't one yet.
/// If there is no memory for more or an error, `NULL` is returned.
/// This is a purely lookup and/or additive operation,
/// so the lifetime of the returned `struct ia2_thread_metadata*` is infinite,
/// and since it's thread-specific,
/// it is thread-safe to read and write.
struct ia2_thread_metadata *ia2_thread_metadata_get_current_thread(void);

struct ia2_addr_location {
  /// A descriptive name of what this address points to.
  /// For example, "stack".
  ///
  /// `NULL` if unknown.
  const char *name;

  /// The metadata of the thread this address belongs to.
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
