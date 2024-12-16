/*
RUN: cat macro_attr_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include <ia2_test_runner.h>
#include "functions.h"

// LINKARGS: --wrap=f
void f(void) {
    cr_log_info("Called `f()`");
}

// LINKARGS: --wrap=g
void g(void) {
    cr_log_info("Called `g()`");
}

// TODO(src_rewriter_wip): this gets --wrap, but i don't think it should
void h(CB cb) {
    cr_log_info("Calling `cb(0)` from `h`");
    cb(0);
}

// LINKARGS: --wrap=i
void i(void) {
    cr_log_info("Called `i()`");
}

// LINKARGS: --wrap=j
void j(void) {
    cr_log_info("Called `j()`");
}

// LINKARGS: --wrap=k
void k(void) {
    cr_log_info("Called `k()`");
}
