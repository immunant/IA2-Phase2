#include <criterion/logging.h>
#include "functions.h"

void f() {
    cr_log_info("Called `f()`");
}

void g() {
    cr_log_info("Called `g()`");
}

// TODO(src_rewriter_wip): this gets --wrap, but i don't think it should
void h(CB cb) {
    cr_log_info("Calling `cb(0)` from `h`");
    cb(0);
}

void i() {
    cr_log_info("Called `i()`");
}

void j() {
    cr_log_info("Called `j()`");
}

void k() {
    cr_log_info("Called `k()`");
}
