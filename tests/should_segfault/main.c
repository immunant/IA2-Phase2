/*
RUN: sh -c 'if [ ! -s "should_segfault_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/
#include <ia2_test_runner.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <print_secret.h>
#include <ia2.h>
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

uint32_t secret = 0xdeadbeef;

// This tests that mpk violations call the signal handler in
// test_fault_handler.h and print the appropriate message if the
// segfault occurred in one of the CHECK_VIOLATION expressions.
// We also check raising a segfault early to test that a violation outside a
// CHECK_VIOLATION results in a exit code of -1 (255).

Test(should_segfault, main) {
    cr_assert(secret);
    print_secret();
}

Test(should_segfault, early_fault, .exit_code = 255) {
    do_early_fault();
    print_secret();
}