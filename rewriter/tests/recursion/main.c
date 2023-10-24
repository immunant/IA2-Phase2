/*
*/

#include "recursion_dso.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <ia2.h>
#include <stdio.h>
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void recurse_main(int count) {
  cr_log_info("recursion_main: %d\n", count);
  if (count > 0) {
    recurse_dso(count - 1);
  }
}

Test(recursion, main) {
  recurse_main(5);
}
