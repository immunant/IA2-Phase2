#include "src.h"
#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

int b IA2_SHARED_DATA;

Test(post_condition, not_b_eq_10, .exit_code = 21) {
  b = 0;
  f1(1, &b);
}

Test(pre_post_condition, success) {
  b = 0;
  f1(10, &b);
}
