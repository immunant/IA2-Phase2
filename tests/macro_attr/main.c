/*
RUN: sh -c 'if [ ! -s "macro_attr_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/
#include "functions.h"
#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

Test(macro_attr, main) {
    f();
    g();
    i();
    j();
    k();
}
