/*
RUN: cp %s %t.h
RUN: cp %s.fns %t.fns
RUN: ia2-header-rewriter --function-allowlist=%t.fns %t.c %t.h -- -I%resource_dir
RUN: cat %t.c.args | FileCheck --check-prefix=LINKARGS %s
RUN: %binary_dir/tests/function_allowlist/function_allowlist-main | diff %S/../Output/function_allowlist.out -
*/

#pragma once

// This function does nothing, but should get wrapped
// LINKARGS: --wrap=foo
void foo();

// This function is not in the allowlist so it should not be wrapped
// LINKARGS-NOT: --wrap=defined_in_main
void defined_in_main();
