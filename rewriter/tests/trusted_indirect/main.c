/*
RUN: cat main.c | FileCheck --match-full-lines --check-prefix=REWRITER %s
RUN: sh -c 'if [ ! -s "trusted_indirect_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
RUN: %binary_dir/tests/trusted_indirect/trusted_indirect_main_wrapped | diff %S/Output/trusted_indirect.out -
RUN: %binary_dir/tests/trusted_indirect/trusted_indirect_main_wrapped clean_exit | diff %S/Output/trusted_indirect.clean_exit.out -
*/
#include <stdio.h>
#include "rand_op.h"
#include <ia2.h>
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

/*
    This program tests that a trusted binary can receive and call function pointers from an
    untrusted shared library.
*/

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

bool clean_exit IA2_SHARED_DATA = false;

// Declare some functions to ensure that we can call a function pointer regardless of whether it
// points to an untrusted shared library or this binary.
uint32_t multiply(uint32_t x, uint32_t y) {
    return x * y;
}

static uint32_t divide(uint32_t x, uint32_t y) {
    return x / y;
}

void call_fn_ptr() {
    function_t f = get_function();
    printf("Got the function %s from the library\n", f.name);
    uint32_t x = 987234;
    uint32_t y = 142151;
    // This calls `f.op` with and without parentheses to ensure the rewriter handles both
    // REWRITER: uint32_t res = IA2_CALL(f.op, 0)(x, y);
    uint32_t res = f.op(x, y);
    printf("%s(%d, %d) = %d\n", f.name, x, y, res);
    // REWRITER: f.op = IA2_FN(multiply);
    f.op = multiply;
    // REWRITER: printf("mul(%d, %d) = %d\n", x, y, IA2_CALL((f.op), 0)(x, y));
    printf("mul(%d, %d) = %d\n", x, y, (f.op)(x, y));
    // REWRITER: f.op = IA2_FN(divide);
    f.op = divide;
    // REWRITER: printf("div(%d, %d) = %d\n", x, y, IA2_CALL(f.op, 0)(x, y));
    printf("div(%d, %d) = %d\n", x, y, f.op(x, y));
}

int main(int argc, char **argv) {
    if (argc > 1) {
        clean_exit = true;
    }
    // Test calling a function pointer with one of the shared library's functions
    call_fn_ptr();

    // Test calling a function pointer with the other shared library function.
    swap_function();
    call_fn_ptr();

    // Test a that segfault occurs if the pointee tries to access memory it shouldn't
    function_t f = get_bad_function();

    static uint32_t secret = 34;
    leak_secret_address(&secret);
    // REWRITER: IA2_CALL((f.op), 0)(0, 0);
    (f.op)(0, 0);
}
