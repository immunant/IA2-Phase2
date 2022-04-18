/*
RUN: cp %s %t.h
RUN: cp %s.fns %t.fns
RUN: ia2-header-rewriter --function-allowlist=%t.fns %t.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
RUN: %binary_dir/tests/function_allowlist/function_allowlist-main | diff %S/../Output/function_allowlist.out -
*/

#pragma once

// This function does nothing, but should get wrapped
// CHECK: IA2_WRAP_FUNCTION(foo);
void foo();

// This function is not in the allowlist so it should not be wrapped
// CHECK-NOT: IA2_WRAP_FUNCTION(defined_in_main)
// CHECK: void defined_in_main();
void defined_in_main();
