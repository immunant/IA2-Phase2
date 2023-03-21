/*
RUN: sh -c 'if [ ! -s "minimal_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
RUN: %binary_dir/tests/minimal/minimal_main_wrapped | diff %S/Output/minimal.out -
RUN: readelf -lW %binary_dir/tests/minimal/minimal_main_wrapped | FileCheck --check-prefix=SEGMENTS %s
*/

// Check that readelf shows exactly one executable segment
// SEGMENTS-COUNT-1: LOAD{{.*}}R E
// SEGMENTS-NOT:     LOAD{{.*}}R E

#include "minimal.h"
#include <ia2.h>

INIT_RUNTIME(1);
INIT_COMPARTMENT(1);

int main() {
    foo();
}
