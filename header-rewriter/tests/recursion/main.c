/*
RUN: cat recursion_call_gates_2.ld | FileCheck --check-prefix=LINKARGS %s
RUN: %binary_dir/tests/recursion/recursion_main_wrapped | diff %binary_dir/tests/recursion/recursion.out -
*/

#include <ia2.h>
#include <stdio.h>
#include "recursion_dso.h"
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

INIT_RUNTIME(2);
INIT_COMPARTMENT(1);

// LINKARGS: --wrap=recurse_main
void recurse_main(int count) {
    printf("recursion_main: %d\n", count);
    if (count > 0) {
        recurse_dso(count - 1);
    }
}

int main() {
    recurse_main(5);

    // We fault in a destructor while exiting, TODO: #112
    expect_fault = true;
}
