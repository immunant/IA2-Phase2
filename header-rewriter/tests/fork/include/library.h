/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
RUN: %binary_dir/tests/fork/fork-main | diff %S/../Output/fork.out -
*/

#pragma once

// This function does nothing, but should get wrapped
// CHECK: IA2_WRAP_FUNCTION(foo);
void foo();
