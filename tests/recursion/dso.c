/*
RUN: cat recursion_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include "recursion_main.h"
#include <ia2_test_runner.h>
#include <ia2.h>
#include <stdio.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

// LINKARGS: --wrap=recurse_dso
void recurse_dso(int count) {
  cr_log_info("recursion_dso: %d\n", count);
  if (count > 0) {
    recurse_main(count - 1);
  }
}
