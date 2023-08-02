/*
RUN: cat trusted_direct_call_gates_0.ld | FileCheck --check-prefix=LINKARGS %s
RUN: %binary_dir/tests/trusted_direct/trusted_direct_main_wrapped | diff %S/Output/trusted_direct.out -
RUN: %binary_dir/tests/trusted_direct/trusted_direct_main_wrapped clean_exit | diff %S/Output/trusted_direct.clean_exit.out -
*/
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <ia2.h>
#include "plugin.h"
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

// This test checks that an untrusted library can call a trusted main binary. An
// MPK violation is triggered from the untrusted library if no arguments are
// passed in. Otherwise the program exits cleanly.

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

uint32_t secret = 0x09431233;

bool clean_exit IA2_SHARED_DATA = false;

//LINKARGS: --wrap=print_message
void print_message(void) {
    printf("%s: the secret 0x%" PRIx32 " is defined in the main binary\n", __func__, secret);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        clean_exit = true;
    }
    start_plugin();
}
