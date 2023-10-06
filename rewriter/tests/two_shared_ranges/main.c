/*
RUN: cat two_shared_ranges_call_gates_2.ld | FileCheck --check-prefix=LINKARGS %s
RUN: readelf -lW %binary_dir/tests/two_shared_ranges/two_shared_ranges_main_wrapped | FileCheck --check-prefix=SEGMENTS %s
*/

// Check that readelf shows exactly one executable segment
// SEGMENTS-COUNT-1: LOAD{{.*}}R E
// SEGMENTS-NOT:     LOAD{{.*}}R E

#include <criterion/criterion.h>
#include <criterion/logging.h>
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
uint32_t shared IA2_SHARED_DATA = 0xb75784ee;

static bool steal_plugin_secret = false;

bool clean_exit IA2_SHARED_DATA = false;

// LINKARGS: --wrap=print_message
void print_message(void) {
    cr_log_info("this is defined in the main binary");
    cr_log_info("the main secret is at %p", &secret);
    cr_log_info("the plugin shared data is at %p", &plugin_shared);
    cr_log_info("the main secret is %x", secret);
    cr_log_info("the plugin shared data is %x", plugin_shared);
    if (steal_plugin_secret) {
        cr_log_info("the plugin secret is %x\n", CHECK_VIOLATION(plugin_secret));
    }
}

Test(two_shared_ranges, main) {
    start_plugin();
}

Test(two_shared_ranges, plugin) {
    steal_plugin_secret = true;
    start_plugin();
}

Test(two_shared_ranges, clean_exit) {
    clean_exit = true;
    start_plugin();
}