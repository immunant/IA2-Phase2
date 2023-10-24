#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <lib.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>

int call_fn(bin_op fn, int x, int y) {
    cr_assert(fn != NULL);
    int res = fn(x, y);
    cr_log_info("%d\n", res);
    return res;
}
