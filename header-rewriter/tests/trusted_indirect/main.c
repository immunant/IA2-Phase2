#include <stdio.h>
#include "rand_op.h"
#include <ia2.h>

/*
    This program tests that a trusted binary can receive and call function pointers from an
    untrusted shared library.
*/

INIT_COMPARTMENT(0);

// Declare some functions to ensure that we can call a function pointer regardless of whether it
// points to an untrusted shared library or this binary.
uint32_t multiply(uint32_t x, uint32_t y) {
    return x * y;
}

static uint32_t divide(uint32_t x, uint32_t y) {
    return x / y;
}

// This program has to redeclare the binary_op typedef with a different identifier because binary_op
// describes opaque function pointers in the rewritten headers.
typedef uint32_t(*bin_op)(uint32_t, uint32_t);

void call_fn_ptr() {
    function_t f = get_function();
    printf("Got the function %s from the library\n", f.name);
    binary_op wrapped_op = f.op;
    bin_op op = IA2_FNPTR_UNWRAPPER(wrapped_op, _ZTSPFjjjE);
    uint32_t x = 987234;
    uint32_t y = 142151;
    printf("%s(%d, %d) = %d\n", f.name, x, y, op(x, y));
    op = multiply;
    printf("mul(%d, %d) = %d\n", x, y, op(x, y));
    op = divide;
    printf("div(%d, %d) = %d\n", x, y, op(x, y));
}

int main() {
    // Test calling a function pointer with one of the shared library's functions
    call_fn_ptr();

    // Test calling a function pointer with the other shared library function.
    swap_function();
    call_fn_ptr();

    // Test a that segfault occurs if the pointee tries to access memory it shouldn't
    function_t f = get_bad_function();
    binary_op wrapped_op = f.op;
    bin_op op = IA2_FNPTR_UNWRAPPER(wrapped_op, _ZTSPFjjjE);

    static uint32_t secret = 34;
    leak_secret_address(&secret);
    op(0, 0);
}
