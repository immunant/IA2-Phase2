#pragma once

#include <stdbool.h>
#include <stdint.h>

struct ia2_destructor_metadata_record {
  const char *wrapper;
  const char *target;
  int compartment_pkey;
  int uses_union_pkru;
};

extern const struct ia2_destructor_metadata_record ia2_destructor_metadata[];
extern const unsigned int ia2_destructor_metadata_count;
extern const int ia2_destructor_exit_pkey;

/// Initialize runtime bookkeeping for destructor metadata.
///
/// Must be called after all compartments have been set up so the rewriter
/// generated symbols are visible via dlsym(). Safe to call multiple times; the
/// function short-circuits if re-invoked.
void ia2_destructor_runtime_init(void);

/// Compute the PKRU mask that should be applied while running a destructor
/// wrapper.
///
/// `wrapper` must be the exact function pointer registered with `__cxa_atexit`.
/// When metadata is present the returned value reflects whether the
/// destructor requires the union (exit compartment + target compartment)
/// permissions; otherwise the caller gets the default single-compartment mask
/// derived from `target_compartment`.
uint32_t ia2_destructor_pkru_for(void (*wrapper)(void), int target_compartment_pkey);

/// Lookup helper primarily intended for diagnostics and tracing.
///
/// Returns true when metadata was found for `wrapper` and fills the out
/// parameters. Callers may pass NULL for any output they do not care about.
bool ia2_destructor_metadata_lookup(void (*wrapper)(void),
                                    const struct ia2_destructor_metadata_record **out_record,
                                    uint32_t *out_pkru_value);

/// Destructor wrapper enter hook - called before switching compartments.
///
/// Reads the current PKRU, computes the desired value for the wrapper based on
/// metadata, applies it, and returns the original PKRU so the caller can
/// restore it afterwards. `wrapper_addr` must point at the wrapper function
/// itself; `target_pkey` is used as a fallback if metadata is unavailable.
uint32_t ia2_destructor_enter(void *wrapper_addr, int target_pkey);

/// Destructor wrapper leave hook - called before returning from wrapper.
///
/// Restores the PKRU value captured by ia2_destructor_enter.
void ia2_destructor_leave(uint32_t original_pkru);
