/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
RUN: %binary_dir/tests/minimal/main > %T/stdout
RUN: diff %S/../Output/minimal.out %T/stdout
*/

#pragma once

// This function does nothing
// CHECK: IA2_WRAP_FUNCTION(foo);
void foo();

// This returns an integer
// CHECK: IA2_WRAP_FUNCTION(return_val);
int return_val();

// This takes an integer
// CHECK: IA2_WRAP_FUNCTION(arg1);
void arg1(int x);
