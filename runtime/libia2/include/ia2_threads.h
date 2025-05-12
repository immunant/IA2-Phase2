#pragma once

#include "ia2.h"

struct ia2_thread_data {
  /// The addresses of each compartment's stack for this thread.
  ///
  /// This data is shared, so it should not be trusted for use as a pointer,
  /// but it can be used best effort as a provenance-less address.
  uintptr_t stack_addrs[IA2_MAX_COMPARTMENTS];
};

struct ia2_addr_location {
  /// A descriptive name of what this address points to.
  /// For example, "stack".
  ///
  /// `NULL` if unknown.
  const char *name;

  /// The thread ID of the thread this address is on.
  ///
  /// `-1` if unknown.
  pid_t tid;

  /// The compartment this address is in.
  ///
  /// `-1` if unknown.
  int compartment;
};

/// Find the `struct ia2_thread_data*` for the current thread,
/// adding (but not allocating) one if there isn't one yet.
/// If there is no memory for more or an error, `NULL` is returned.
/// This is a purely lookup and/or additive operation,
/// so the lifetime of the returned `struct ia2_thread_data*` is infinite,
/// and since it's thread-specific,
/// it is thread-safe to read and write.
struct ia2_thread_data *ia2_thread_data_get_current_thread(void);

/// Find the `ia2_addr_location` of `addr`.
/// If it is not found or there is an error,
/// the fields are set to `NULL` or `-1` depending on the type.
struct ia2_addr_location ia2_addr_location_find(uintptr_t addr);
