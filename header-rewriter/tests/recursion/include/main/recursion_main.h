/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %T/wrapper.c.args | FileCheck --check-prefix=LINKARGS %s
RUN: %binary_dir/tests/recursion/recursion-main | diff %binary_dir/tests/recursion/recursion.out -
*/

#pragma once

// LINKARGS: --wrap=recurse_main
void recurse_main(int count);
