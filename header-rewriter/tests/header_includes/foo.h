#pragma once
#include "bar.h"
#include "baz.h"

/* CHECK: IA2_WRAP_FUNCTION(foo) */
struct Option Some(int x);

int foo2();
