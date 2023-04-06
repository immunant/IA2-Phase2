/*
RUN: cat rewrite_fn_ptr_eq_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <lib.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>

// LINKARGS: --wrap=call_fn
int call_fn(bin_op fn, int x, int y) {
    assert(fn != NULL);
    int res = fn(x, y);
    printf("%d\n", res);
    return res;
}
