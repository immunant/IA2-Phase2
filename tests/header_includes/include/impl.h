#pragma once
#include "types.h"

// LINKARGS: --wrap=unwrap_or
int unwrap_or(Option opt, int default_value);
