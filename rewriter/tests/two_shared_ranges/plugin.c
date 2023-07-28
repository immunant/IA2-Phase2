/*
RUN: cat two_shared_ranges_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
RUN: readelf -lW %binary_dir/tests/two_shared_ranges/libtwo_shared_ranges_lib_wrapped.so | FileCheck --check-prefix=SEGMENTS %s
*/

// Check that readelf shows exactly one executable segment
// SEGMENTS-COUNT-1: LOAD{{.*}}R E
// SEGMENTS-NOT:     LOAD{{.*}}R E

#include <stdio.h>
#include <ia2.h>
#include "exported_fn.h"
#include "test_fault_handler.h"

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

uint32_t plugin_secret = 0x78341244;
uint32_t plugin_shared IA2_SHARED_DATA = 0x415ea635;

extern bool clean_exit;

// LINKARGS: --wrap=start_plugin
void start_plugin(void) {
    LOG("this is defined in the plugin");
    if (debug_mode) {
        LOG("the plugin secret is at %p", &plugin_secret);
        LOG("the main shared data is at %p", &shared);
    }
    LOG("the plugin secret is %x", plugin_secret);
    LOG("the main shared data is %x", shared);
    print_message();
    if (!clean_exit) {
        LOG("the main secret is %x", CHECK_VIOLATION(secret));
    }
}
