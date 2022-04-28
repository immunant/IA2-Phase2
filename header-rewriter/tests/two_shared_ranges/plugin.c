#include <stdio.h>
#include <ia2.h>
#include "exported_fn.h"
#include "test_fault_handler.h"

INIT_COMPARTMENT(2);

uint32_t plugin_secret = 0x78341244;
uint32_t plugin_shared IA2_SHARED_DATA = 0x415ea635;

extern bool clean_exit;

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
