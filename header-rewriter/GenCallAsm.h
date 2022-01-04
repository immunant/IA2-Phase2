#pragma once
#include "CAbi.h"

auto emit_call_asm(const CAbiSignature& sig, const std::string& name) -> std::string;
