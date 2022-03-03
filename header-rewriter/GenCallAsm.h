#pragma once
#include "CAbi.h"

std::string emit_asm_wrapper(const CAbiSignature &sig, const std::string &name,
                             bool indirect_wrapper);
