/*
RUN: cat main.c | FileCheck --match-full-lines --check-prefix=REWRITER %S/main.c
*/
#include <criterion/criterion.h>
#include <stdio.h>
#include <lib.h>
#include <ia2.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

int add(int x, int y) {
    return x + y;
}

__attribute__((used))
static int sub(int x, int y) {
    return x - y;
}

struct module {
    bin_op fn;
};

Test(rewrite_fn_ptr_eq, main) {
    int res;
    int *y = &res;
    void *x = NULL;
    bin_op fn = add;
    bin_op fn2 = NULL;

    // Check that pointers for types other than functions are not rewritten
    // REWRITER: if (y) { }
    if (y) { }
    // REWRITER: if (x) { }
    if (x) { }
    // REWRITER: if (!y) { }
    if (!y) { }
    // REWRITER: if (!x) { }
    if (!x) { }

    // REWRITER: if (IA2_ADDR(fn)) {
    if (fn) {
        call_fn(fn, 2, 1);
    }
    // REWRITER: if (!IA2_ADDR(fn2)) {
    if (!fn2) {
        fn2 = sub;
        call_fn(fn2, 2, 1);
    }
    struct module mod = { add };
    if (mod.fn) { }

    struct module *mod_ptr = &mod;
    if (mod_ptr->fn) { }

    // REWRITER: res = IA2_ADDR(fn) ? IA2_CALL(fn, _ZTSFiiiE)(1, 2) : -1;
    res = fn ? fn(1, 2) : -1;

    // REWRITER: res = !IA2_ADDR(fn) ? -1 : IA2_CALL(fn, _ZTSFiiiE)(1, 2);
    res = !fn ? -1 : fn(1, 2);

    // REWRITER: res = x && IA2_ADDR(fn) ? IA2_CALL(fn, _ZTSFiiiE)(1, 2) : -1;
    res = x && fn ? fn(1, 2) : -1;

    // REWRITER: res = IA2_ADDR(fn) && IA2_ADDR(fn2) ? IA2_CALL(fn, _ZTSFiiiE)(1, 2) + IA2_CALL(fn2, _ZTSFiiiE)(1, 2) : -1;
    res = fn && fn2 ? fn(1, 2) + fn2(1, 2) : -1;

    bin_op *ptr = &fn;
    // REWRITER: if (IA2_ADDR(*ptr)) { }
    if (*ptr) { }

    // REWRITER: if (NULL == IA2_ADDR(mod_ptr->fn)) { }
    if (NULL == mod_ptr->fn) { }
    // REWRITER: if (IA2_ADDR(mod.fn) != NULL) { }
    if (mod.fn != NULL) { }

    // REWRITER: if (IA2_ADDR(mod.fn) == IA2_ADDR(mod_ptr->fn)) { }
    if (mod.fn == mod_ptr->fn) { }

    // REWRITER: if (IA2_ADDR(mod.fn) == IA2_FN_ADDR(add)) { }
    if (mod.fn == add) { }
    // REWRITER: if (IA2_FN_ADDR(sub) == IA2_ADDR(mod_ptr->fn)) { }
    if (sub == mod_ptr->fn) { }

    // REWRITER: if (x && IA2_ADDR(fn2)) { }
    if (x && fn2) { }
    // REWRITER: if (y || !IA2_ADDR(fn)) { }
    if (y || !fn) { }
    // REWRITER: if (x && IA2_ADDR(fn) && y) { }
    if (x && fn && y) { }
}
