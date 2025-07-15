#include "src.h"
#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
    ia2_register_compartment("libpre_post_condition_lib.so", 2, NULL);
}

int b IA2_SHARED_DATA;

Test(pre_condition, not_a_positive, .exit_code = 10) {
  b = 0;
  f2(-1, &b);
}

Test(pre_condition, not_b_non_null, .exit_code = 11) {
  f2(1, NULL);
}

Test(pre_condition, not_b_eq_0, .exit_code = 13) {
  b = 1;
  f2(1, &b);
}

Test(post_condition, not_b_eq_10, .exit_code = 21) {
  b = 0;
  f2(1, &b);
}

Test(pre_post_condition, success) {
  b = 0;
  f2(10, &b);
}

Test(pre_post_condition, odd_num_args_1) {
  f1(11);
}

Test(pre_post_condition, odd_num_args_3) {
  b = 0;
  f3(&b, 11, 22);
}
