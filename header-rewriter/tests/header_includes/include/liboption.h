#pragma once
#include "types.h"
#include "impl.h"

// LINKARGS: --wrap=Some
Option Some(int x);

// LINKARGS: --wrap=None
Option None();
