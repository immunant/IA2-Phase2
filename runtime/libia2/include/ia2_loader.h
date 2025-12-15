#ifndef IA2_LOADER_H
#define IA2_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Thread-local flag indicating loader context. In this trimmed port it is
// provided by a stub and always false unless a future loader gate sets it.
#if defined(IA2_ENABLE) && IA2_ENABLE && defined(IA2_LIBC_COMPARTMENT) && IA2_LIBC_COMPARTMENT
#ifdef __cplusplus
extern thread_local bool ia2_in_loader_gate;
#else
extern _Thread_local bool ia2_in_loader_gate;
#endif

// Stubbed gate enter/exit for builds with dynamic loader gating enabled.
void ia2_loader_gate_enter(void);
void ia2_loader_gate_exit(void);
#else
// In non-libc-compartment builds, provide header-only no-ops so references
// compile and link without pulling in the stub object.
static const bool ia2_in_loader_gate = false;
static inline void ia2_loader_gate_enter(void) {}
static inline void ia2_loader_gate_exit(void) {}
#endif

#ifdef __cplusplus
}
#endif

#endif // IA2_LOADER_H
