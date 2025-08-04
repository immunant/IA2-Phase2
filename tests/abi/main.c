/*
RUN: sh -c 'if [ ! -s "minimal_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/

// Check that readelf shows exactly one executable segment

#include "abi.h"
#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
}

Test(abi, main) {
  cr_log_info("Calling foo");
  foo();
  arg1(1);
  arg2(1, 2);
  arg3(1, 1.0, 2);
  many_args(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

  struct in_memory im = {0};
  im.arr[0] = 1;
  arg_in_memory(im);
  im = ret_in_memory(2);
  cr_assert(2 == im.arr[0]);
  cr_assert(1 == return_val());

  fn_ptr_ret_in_mem fn = ret_in_memory;
  im = fn(1);

  fn_ptr_many_args fn2 = many_args;
  fn2(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}
