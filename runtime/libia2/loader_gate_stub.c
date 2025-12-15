#include <ia2_loader.h>

// Stub definition used when loader gating is not implemented. The flag remains
// false so allocation paths treat the loader like any other caller.
_Thread_local bool ia2_in_loader_gate = false;

void ia2_loader_gate_enter(void) { ia2_in_loader_gate = true; }

void ia2_loader_gate_exit(void) { ia2_in_loader_gate = false; }
