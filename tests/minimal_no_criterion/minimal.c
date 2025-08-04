/*
RUN: cat minimal_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include "minimal.h"
#include <stdio.h>

// LINKARGS: --wrap=arg1
void arg1(int x) {
  printf("arg1");
}

// LINKARGS: --wrap=foo
void foo() {
  printf("foo");
}

// LINKARGS: --wrap=return_val
int return_val() {
  printf("return_val");
  return 1;
}
