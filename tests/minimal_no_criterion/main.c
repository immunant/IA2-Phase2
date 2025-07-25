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

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
}

int main() {
    printf("Calling foo");
    foo();
}
