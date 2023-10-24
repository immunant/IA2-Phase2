#include <criterion/criterion.h>
#include <criterion/logging.h>
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
    cr_log_info("Got the function %s from the library\n", f.name);
    uint32_t x = 987234;
    uint32_t y = 142151;
    // This calls `f.op` with and without parentheses to ensure the rewriter handles both
    uint32_t res1 = f.op(x, y);
    f.op = multiply;
    uint32_t res2 = (f.op)(x, y);
    cr_assert_eq(res2, 2897346862);
    f.op = divide;
    uint32_t res3 = f.op(x, y);
    cr_assert_eq(res3, 6);
}

void do_test() {
    // Test calling a function pointer with one of the shared library's functions
    call_fn_ptr();

    // Test calling a function pointer with the other shared library function.
    swap_function();
    call_fn_ptr();

    // Test a that segfault occurs if the pointee tries to access memory it shouldn't
    function_t f = get_bad_function();

    static uint32_t secret = 34;
    leak_secret_address(&secret);
    (f.op)(0, 0);
}

Test(trusted_indirect, no_clean_exit) {
    clean_exit = false;
    do_test();
}

Test(trusted_indirect, clean_exit) {
    clean_exit = true;
    do_test();
}
