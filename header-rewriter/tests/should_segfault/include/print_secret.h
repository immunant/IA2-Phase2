/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
RUN: %binary_dir/tests/should_segfault/should_segfault-main | diff %S/../Output/should_segfault.out -
*/
#pragma once
#include <stdint.h>

extern uint32_t secret;

// CHECK: IA2_WRAP_FUNCTION(print_secret);
void print_secret();
