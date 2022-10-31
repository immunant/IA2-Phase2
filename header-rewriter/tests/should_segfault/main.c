#define IA2_INIT_COMPARTMENT 1
#include <ia2.h>

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <print_secret.h>
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

INIT_RUNTIME(1);

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
