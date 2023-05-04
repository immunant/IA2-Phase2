/*
RUN: sh -c 'if [ ! -s "macro_attr_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/
#include "functions.h"
#include <ia2.h>

INIT_RUNTIME(1);
INIT_COMPARTMENT(1);

int main() {
    f();
    g();
    i();
    j();
    k();
}
