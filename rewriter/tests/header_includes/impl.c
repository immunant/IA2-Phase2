/*
RUN: cat header_includes_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include "types.h"

// LINKARGS: --wrap=unwrap_or
int unwrap_or(Option opt, int default_value) {
    if (opt.present) {
        return opt.value;
    } else {
        return default_value;
    }
}
