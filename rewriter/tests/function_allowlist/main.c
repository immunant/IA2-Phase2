/*
RUN: cat function_allowlist_call_gates_0.ld | FileCheck --check-prefix=LINKARGS %s
RUN: %binary_dir/tests/function_allowlist/function_allowlist_main_wrapped | diff %S/Output/function_allowlist.out -
*/
#include "library.h"
#include <ia2.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

int data_in_main = 30;

// LINKARGS: --wrap=defined_in_main
void defined_in_main() { printf("data is %d\n", data_in_main); }

int main() {
  foo();
  defined_in_main();
}
