#pragma once
#include "CAbi.h"

/* The kind of call to generate a wrapper */
enum class WrapperKind {
    // Direct call to another compartment
    Direct,
    // Indirect call through a pointer received from another compartment
    IndirectFromTrusted,
    // Indirect call through a pointer sent to another compartment
    IndirectFromUntrusted,
};

std::string emit_asm_wrapper(const CAbiSignature &sig, const std::string &name,
                             WrapperKind kind);
