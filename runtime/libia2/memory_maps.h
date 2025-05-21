#pragma once

#include "ia2.h"
#include "ia2_memory_maps.h"

#include <pthread.h>

#if IA2_DEBUG_LOG

/// The data here is shared, so it should not be trusted for use as a pointer,
/// but it can be used best effort for non-trusted purposes.
struct ia2_thread_metadata {
  pid_t tid;
  pthread_t thread;

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

/// Find the `struct ia2_thread_metadata*` for the current thread,
/// adding (but not allocating) one if there isn't one yet.
/// If there is no memory for more or an error, `NULL` is returned.
/// This is a purely lookup and/or additive operation,
/// so the lifetime of the returned `struct ia2_thread_metadata*` is infinite,
/// and since it's thread-specific,
/// it is thread-safe to read and write.
struct ia2_thread_metadata *ia2_thread_metadata_get_current_thread(void);

#endif

struct ia2_addr_location {
  /// A descriptive name of what this address points to.
  /// For example, "stack".
  ///
  /// `NULL` if unknown.
  const char *name;

  /// The thread ID of the thread this address belongs to.
  ///
  /// `-1` if unknown.
  pid_t tid;

  /// The `pthread_t` of the thread this address belongs to.
  ///
  /// If `tid` is `-1`, this is not initialized.
  pthread_t thread;

  /// The compartment this address is in.
  ///
  /// `-1` if unknown.
  int compartment;
};

/// Find the `ia2_addr_location` of `addr`.
/// If it is not found or there is an error,
/// the fields are set to `NULL` or `-1` depending on the type.
struct ia2_addr_location ia2_addr_location_find(uintptr_t addr);
