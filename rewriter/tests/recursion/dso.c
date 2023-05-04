/*
RUN: cat recursion_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include "recursion_main.h"
#include <ia2.h>
#include <stdio.h>

INIT_COMPARTMENT(2);

// LINKARGS: --wrap=recurse_dso
void recurse_dso(int count) {
    printf("recursion_dso: %d\n", count);
    if (count > 0) {
        recurse_main(count - 1);
    }
}
