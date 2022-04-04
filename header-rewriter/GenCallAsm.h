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

// Generates a wrapper for a function named \p name with the signature \p sig.
// The WrapperKind parameter \p kind determines the type of call which may
// affect the order of operations and the layout of the wrapper stack frame.
//
// The caller's protection key index is fixed at compile-time (not generation-
// time) and available via a preprocessor macro or preprocessor macro parameter,
// depending on the wrapper kind.
// \p callee_pkey is a string giving the index of the callee's protection key,
// which must be valid to pass to the `PKRU` macro in ia2.h.
// \p as_macro determines if the wrappers for direct calls is emitted as a
// macro. Indirect calls are unconditionally emitted as macros.
std::string emit_asm_wrapper(const CAbiSignature &sig, const std::string &name,
                             WrapperKind kind, const std::string &callee_pkey,
                             bool as_macro = false);
