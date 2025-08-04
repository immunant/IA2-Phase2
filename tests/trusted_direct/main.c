/*
RUN: cat trusted_direct_call_gates_0.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <ia2_test_runner.h>

#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "plugin.h"
#include <ia2.h>

// This test checks that an untrusted library can call a trusted main binary. An
// MPK violation is triggered from the untrusted library if no arguments are
// passed in. Otherwise the program exits cleanly.

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
}

uint32_t secret = 0x09431233;

bool clean_exit IA2_SHARED_DATA = false;

// LINKARGS: --wrap=print_message
void print_message(void) {
  cr_log_info("%s: the secret 0x%" PRIx32 " is defined in the main binary\n", __func__, secret);
  cr_assert(secret == 0x09431233);
}

Test(trusted_direct, no_clean_exit) {
  clean_exit = false;
  start_plugin();
}

Test(trusted_direct, clean_exit) {
  clean_exit = true;
  start_plugin();
}
