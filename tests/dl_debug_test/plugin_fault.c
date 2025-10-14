#include <ia2.h>

#define IA2_COMPARTMENT 3
#include <ia2_compartment_init.inc>

static int plugin_guard;

void plugin_fault_write(void) {
    plugin_guard = 1;
}
