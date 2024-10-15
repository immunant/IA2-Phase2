/*
RUN: sh -c 'if [ ! -s "header_includes_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/
#include "liboption.h"
#include "types.h"
#include <ia2_test_runner.h>

#include <ia2.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

Test(header_includes, main) {
    Option x = Some(3);
    Option none = None();
    cr_assert_eq(unwrap_or(x, -1), 3);
    cr_assert_eq(unwrap_or(none, -1), -1);
}
