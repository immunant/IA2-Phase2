#include <ia2_test_runner.h>

#include <stdio.h>
#include <ia2.h>
#include "exported_fn.h"


#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

uint32_t plugin_secret = 0x78341244;

extern bool clean_exit;

void start_plugin(void) {
    cr_log_info("this is defined in the plugin");
    if (debug_mode) {
        cr_log_info("the plugin secret is at %p", &plugin_secret);
    }
    cr_assert(plugin_secret == 0x78341244);
    print_message();
    if (!clean_exit) {
        cr_assert(CHECK_VIOLATION(secret) == 0x09431233);
    }
}

void exit_from_plugin(void) {
    exit(0);
}
