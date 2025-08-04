/*
RUN: cat recursion_call_gates_2.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include "recursion_dso.h"
#include <ia2_test_runner.h>

#include <ia2.h>
#include <stdio.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
  ia2_register_compartment("librecursion_lib.so", 2, NULL);
}

// LINKARGS: --wrap=recurse_main
void recurse_main(int count) {
  cr_log_info("recursion_main: %d\n", count);
  if (count > 0) {
    recurse_dso(count - 1);
  }
}

Test(recursion, main) {
  recurse_main(5);
}
