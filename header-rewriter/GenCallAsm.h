#pragma once
#include "CAbi.h"

auto emit_asm_wrapper(const CAbiSignature &sig, const std::string &name)
    -> std::string;
