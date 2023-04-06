#include <lib.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>

int call_fn(bin_op fn, int x, int y) {
    assert(fn != NULL);
    int res = fn(x, y);
    printf("%d\n", res);
    return res;
}
