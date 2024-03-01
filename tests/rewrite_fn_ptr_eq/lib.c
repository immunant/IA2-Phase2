/*
RUN: cat rewrite_fn_ptr_eq_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <lib.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>

// LINKARGS: --wrap=call_fn
int call_fn(bin_op fn, int x, int y) {
    cr_assert(fn != NULL);
    int res = fn(x, y);
    cr_log_info("%d\n", res);
    return res;
}
