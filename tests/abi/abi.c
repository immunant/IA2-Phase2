/*
RUN: cat minimal_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include "abi.h"
#include <ia2_test_runner.h>

// LINKARGS: --wrap=arg1
void arg1(int x) {
  cr_assert(x == 1);
  cr_log_info("arg1");
}

// LINKARGS: --wrap=foo
void foo(void) {
    cr_log_info("foo");
}

// LINKARGS: --wrap=return_val
int return_val(void) {
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

void many_args(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
  cr_assert(a == 1);
  cr_assert(b == 2);
  cr_assert(c == 3);
  cr_assert(d == 4);
  cr_assert(e == 5);
  cr_assert(f == 6);
  cr_assert(g == 7);
  cr_assert(h == 8);
  cr_assert(i == 9);
  cr_assert(j == 10);
  cr_log_info("many_args");
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