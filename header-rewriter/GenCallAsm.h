#pragma once
#include "CAbi.h"

/* The kind of call to generate a wrapper for */
enum class WrapperKind {
  // Direct call to another compartment
  Direct,
  // Indirect call through a pointer received from another compartment
  IndirectFromTrusted,
  // Indirect call through a pointer sent to another compartment
  IndirectFromUntrusted,
};

// Generates a wrapper for a function with the given signature and name.
// WrapperKind determines the type of call which may affect the order of
// operations and the layout of the wrapper stack frame. compartment_pkey is
// the callee's protection key. The caller's protection key is always a macro
// definition or parameter which is determined by the type of call.
std::string emit_asm_wrapper(const CAbiSignature &sig, const std::string &name,
                             WrapperKind kind,
                             const std::string &callee_pkey);
