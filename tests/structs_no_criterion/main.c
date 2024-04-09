/*
RUN: sh -c 'if [ ! -s "minimal_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/

// Check that readelf shows exactly one executable segment

#include <stdio.h>
#include "minimal.h"
#include <ia2.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

int main() {
    printf("Calling foo");
    struct s1 x = foo();
}
