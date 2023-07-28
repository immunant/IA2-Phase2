/*
RUN: sh -c 'if [ ! -s "should_segfault_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
RUN: %binary_dir/tests/should_segfault/should_segfault_main_wrapped | diff %binary_dir/tests/should_segfault/should_segfault.out -
RUN: %binary_dir/tests/should_segfault/should_segfault_main_wrapped early_fault | diff %binary_dir/tests/should_segfault/early_segfault.out -
*/
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
// segfault occurred in one of the CHECK_VIOLATION expressions. Passing in any
// argument raises a segfault early to test that a violation outside a
// CHECK_VIOLATION prints a different message.
int main(int argc, char **argv) {
    if (argc > 1) {
        do_early_fault();
    }
    printf("TRUSTED: the secret is %x\n", secret);
    print_secret();
}
