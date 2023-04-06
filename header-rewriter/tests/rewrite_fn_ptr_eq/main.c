/*
RUN: cat main.c | FileCheck --match-full-lines --check-prefix=REWRITER %S/main.c
*/
#include <stdio.h>
#include <lib.h>
#include <ia2.h>

INIT_RUNTIME(1);
INIT_COMPARTMENT(1);

int add(int x, int y) {
    return x + y;
}

__attribute__((used))
static int sub(int x, int y) {
    return x - y;
}

int main() {
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
}
