#pragma once
#include "types.h"
#include "impl.h"

// CHECK: IA2_WRAP_FUNCTION(Some);
Option Some(int x);

// CHECK: IA2_WRAP_FUNCTION(None);
Option None();
