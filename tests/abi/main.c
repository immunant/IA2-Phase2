/*
RUN: sh -c 'if [ ! -s "minimal_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/

// Check that readelf shows exactly one executable segment

#include <criterion/criterion.h>
#include "abi.h"
#include <ia2.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

Test(abi, main) {
    cr_log_info("Calling foo");
    foo();
    arg1(1);
    arg2(1, 2);
    arg3(1, 1.0, 2);

    struct in_memory im = {0};
    im.arr[0] = 1;
    arg_in_memory(im);
    im = ret_in_memory(1);
    cr_assert(1 == return_val());

    fn_ptr_ret_in_mem fn = ret_in_memory;
    im = fn(1);
}