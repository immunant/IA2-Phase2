/*
*/

// Check that readelf shows exactly one executable segment

#include <criterion/criterion.h>
#include "minimal.h"
#include <ia2.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

Test(minimal, main) {
    cr_log_info("Calling foo");
    foo();
}
