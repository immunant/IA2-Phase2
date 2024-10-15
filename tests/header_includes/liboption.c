/*
RUN: cat header_includes_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include <ia2_test_runner.h>
#include "liboption.h"
#include "types.h"

// LINKARGS: --wrap=None
Option None() {
    cr_log_info("returning `None`");
    Option none = {
        .value = 0,
        .present = false,
    };
    return none;
}

// LINKARGS: --wrap=Some
Option Some(int x) {
    cr_log_info("returning `Some(%d)`", x);
    Option opt = {
        .value = x,
        .present = true,
    };
    return opt;
}
