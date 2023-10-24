
// Check that readelf shows exactly one executable segment

#include <criterion/logging.h>
#include <ia2.h>
#include "exported_fn.h"
#include "test_fault_handler.h"

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

uint32_t plugin_secret = 0x78341244;
uint32_t plugin_shared IA2_SHARED_DATA = 0x415ea635;

extern bool clean_exit;

void start_plugin(void) {
    cr_log_info("this is defined in the plugin");
    cr_log_info("the plugin secret is at %p", &plugin_secret);
    cr_log_info("the main shared data is at %p", &shared);
    cr_log_info("the plugin secret is %x", plugin_secret);
    cr_log_info("the main shared data is %x", shared);
    print_message();
    if (!clean_exit) {
        cr_log_info("the main secret is %x", CHECK_VIOLATION(secret));
    }
}
