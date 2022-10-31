#define IA2_INIT_COMPARTMENT 1
#include <ia2.h>

#include <stdio.h>
#include "rand_op.h"
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

/*
    This program tests that a trusted binary can receive and call function pointers from an
    untrusted shared library.
*/

INIT_RUNTIME(1);

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
    printf("%s(%d, %d) = %d\n", f.name, x, y, IA2_CALL(f.op, _ZTSPFjjjE, 1)(x, y));
    f.op = IA2_DEFINE_WRAPPER_FN_SCOPE(multiply, _ZTSPFjjjE, 1);
    printf("mul(%d, %d) = %d\n", x, y, IA2_CALL(f.op, _ZTSPFjjjE, 1)(x, y));
    f.op = IA2_DEFINE_WRAPPER_FN_SCOPE(divide, _ZTSPFjjjE, 1);
    printf("div(%d, %d) = %d\n", x, y, IA2_CALL(f.op, _ZTSPFjjjE, 1)(x, y));
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
    IA2_CALL(f.op, _ZTSPFjjjE, 1)(0, 0);
}
