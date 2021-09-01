#pragma once
#include "bar.h"
#include "baz.h"

/* CHECK: IA2_WRAP_FUNCTION(foo) */
void foo(int x);

/* CHECK: IA2_WRAP_FUNCTION(foo2) */
int foo2();
