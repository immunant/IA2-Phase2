#pragma once

#include "ia2.h"
#include "ia2_memory_maps.h"

#include <pthread.h>

void setup_thread_metadata(void);

// Only enable this code that stores these addresses when debug logging is enabled.
// This reduces the trusted codebase and avoids runtime overhead.
#if IA2_DEBUG_MEMORY

/// The data here is shared, so it should not be trusted for use as a pointer,
/// but it can be used best effort for non-trusted purposes.
///
/// All fields should be used atomically.
struct ia2_thread_metadata {
  pid_t tid;
  pthread_t thread;

  /// The start function passed to `pthread_create`.
  void *(*start_fn)(void *arg);

  /// The addresses of each compartment's stack for this thread.
  uintptr_t stack_addrs[IA2_MAX_COMPARTMENTS];

  /// The addresses of each compartment's TLS region for this thread,
  /// except for compartment 1, which has split TLS regions (see below).
  uintptr_t tls_addrs[IA2_MAX_COMPARTMENTS];

  /// The TLS region is split only for the first compartment,
  /// so we need two addresses for just that one.
  ///
  /// Compartment 1's TLS region is split because there is a page of
  /// unprotected data for `ia2_stackptr_0` (in compartment 0), plus padding,
  /// as we don't have a general implementation of shared TLS yet,
  /// but `ia2_stackptr_0` is special-cased for now
  /// as it must be stored in TLS and unprotected.
  uintptr_t tls_addr_compartment1_first;
  uintptr_t tls_addr_compartment1_second;
};

/// Allocate and initialize a new `ia2_thread_metadata` for the current thread.
/// Importantly, this may only be called once per thread.
///
/// The returned pointer is stable, non-`NULL`,
/// and will not be moved or deallocated/uninitialized.
/// Operations on the `ia2_thread_metadata` must be atomic.
///
/// If too many threads are created, this will abort and `MAX_THREADS` can be increased.
struct ia2_thread_metadata *ia2_thread_metadata_new_for_current_thread(void);

/// Find the `ia2_thread_metadata` for the current thread.
///
/// `ia2_thread_metadata_new_for_current_thread`
/// must have been previously called for this thread,
/// or else the `ia2_thread_metadata` will not be found and this will abort.
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
