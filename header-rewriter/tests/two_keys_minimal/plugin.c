/*
RUN: cat two_keys_minimal_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
RUN: readelf -lW %binary_dir/tests/two_keys_minimal/libtwo_keys_minimal_lib_wrapped.so | FileCheck --check-prefix=SEGMENTS %s
*/

// Check that readelf shows exactly one executable segment
// SEGMENTS-COUNT-1: LOAD{{.*}}R E
// SEGMENTS-NOT:     LOAD{{.*}}R E

#include <stdio.h>
#include <ia2.h>
#include "exported_fn.h"
#include "test_fault_handler.h"

INIT_COMPARTMENT(2);

uint32_t plugin_secret = 0x78341244;

extern bool clean_exit;

// LINKARGS: --wrap=start_plugin
void start_plugin(void) {
    LOG("this is defined in the plugin");
    if (debug_mode) {
        LOG("the plugin secret is at %p", &plugin_secret);
    }
    LOG("the plugin secret is %x", plugin_secret);
    print_message();
    if (!clean_exit) {
        LOG("the main secret is %x", CHECK_VIOLATION(secret));
    }
}
