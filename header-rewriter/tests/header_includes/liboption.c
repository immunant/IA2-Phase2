/*
RUN: cat header_includes_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <stdio.h>
#include "liboption.h"
#include "types.h"

// LINKARGS: --wrap=None
Option None() {
    printf("returning `None`\n");
    Option none = {
        .value = 0,
        .present = false,
    };
    return none;
}

// LINKARGS: --wrap=Some
Option Some(int x) {
    printf("returning `Some(%d)`\n", x);
    Option opt = {
        .value = x,
        .present = true,
    };
    return opt;
}
