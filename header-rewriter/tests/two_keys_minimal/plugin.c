#include <stdio.h>
#include <ia2.h>
#include "exported_fn.h"
#include "test_fault_handler.h"

INIT_COMPARTMENT(1);

uint32_t plugin_secret = 0x78341244;

extern bool clean_exit;

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
