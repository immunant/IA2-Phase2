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
    if (y) { }
    if (x) { }
    if (!y) { }
    if (!x) { }

    if (fn) {
        call_fn(fn, 2, 1);
    }
    if (!fn2) {
        fn2 = sub;
        call_fn(fn2, 2, 1);
    }
    struct module mod = { add };
    if (mod.fn) { }

    struct module *mod_ptr = &mod;
    if (mod_ptr->fn) { }

    bin_op *ptr = &fn;
    if (*ptr) { }

    if (NULL == mod_ptr->fn) { }
    if (mod.fn != NULL) { }

    if (mod.fn == mod_ptr->fn) { }

    if (mod.fn == add) { }
    if (sub == mod_ptr->fn) { }

    if (x && fn2) { }
    if (y || !fn) { }
    if (x && fn && y) { }
}
