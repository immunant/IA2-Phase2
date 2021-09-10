#pragma once
#include "baz.h"

struct Option {
    struct X x;
    enum Bool present;
};

/* CHECK: IA2_WRAP_FUNCTION(bar) */
int bar();
