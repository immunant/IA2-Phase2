/*
RUN: sh -c 'if [ ! -s "should_segfault_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
RUN: cat two_keys_minimal_call_gates_2.ld | FileCheck --check-prefix=LINKARGS %s
*/

// Check that readelf shows exactly one executable segment

#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <stdio.h>
#include <unistd.h>
#include <ia2.h>
#include "plugin.h"
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

// This test uses two protection keys
INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

uint32_t secret = 0x09431233;

static bool steal_plugin_secret = false;
// Running in debug mode prints the addresses of the secrets defined in each
// compartment. This is off by default to simplify the diff of stdout against
// the expected output.
bool debug_mode IA2_SHARED_DATA = false;

bool clean_exit IA2_SHARED_DATA = false;

// LINKARGS: --wrap=print_message
void print_message(void) {
    cr_log_info("this is defined in the main binary");
    if (debug_mode) {
        cr_log_info("the main secret is at %p", &secret);
    }
    cr_assert(secret == 0x09431233);
    if (steal_plugin_secret) {
        cr_assert(CHECK_VIOLATION(plugin_secret) == 0x78341244);
    }
}

Test(two_keys, main) {
    start_plugin();
}

Test(two_keys, plugin) {
    steal_plugin_secret = true;
    start_plugin();
}

Test(two_keys, clean_exit) {
    clean_exit = true;
    start_plugin();
}
