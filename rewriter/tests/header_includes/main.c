/*
RUN: sh -c 'if [ ! -s "header_includes_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/
#include "liboption.h"
#include "types.h"
#include <stdio.h>
#include <ia2.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

int main() {
    Option x = Some(3);
    Option none = None();
    printf("`x` has value %d\n", unwrap_or(x, -1));
    printf("`none` has no value %d\n", unwrap_or(none, -1));
}
