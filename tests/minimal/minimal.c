/*
RUN: cat minimal_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include "minimal.h"
#include <ia2_test_runner.h>

// LINKARGS: --wrap=arg1
void arg1(int x) {
  cr_log_info("arg1");
}

// LINKARGS: --wrap=foo
void foo() {
  cr_log_info("foo");
}

// LINKARGS: --wrap=return_val
int return_val() {
  cr_log_info("return_val");
  return 1;
}
