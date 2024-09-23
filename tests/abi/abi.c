/*
RUN: cat minimal_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include "abi.h"
#include <criterion/criterion.h>

// LINKARGS: --wrap=arg1
void arg1(int x) {
  cr_assert(x == 1);
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

void arg2(int x, int y) {
  cr_assert(x == 1);
  cr_assert(y == 2);
  cr_log_info("arg2");
}

void arg3(int x, float f, int y) {
  cr_assert(x == 1);
  cr_assert(y == 2);
  cr_log_info("arg3");
}

void arg_in_memory(struct in_memory im) {
  cr_assert(im.arr[0] == 1);
  cr_log_info("arg_in_memory");
}

struct in_memory ret_in_memory(int x) {
  struct in_memory im;
  im.arr[0] = x;
  cr_log_info("ret_in_memory");
  return im;
}