#pragma once

struct X {
    int value;
};

enum Bool {
    false,
    true,
};

/* CHECK: IA2_WRAP_FUNCTION(baz) */
int baz();
