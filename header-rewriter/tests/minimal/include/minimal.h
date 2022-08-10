/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%resource_dir
RUN: cat %t.c.args | FileCheck --check-prefix=LINKARGS %s
RUN: %binary_dir/tests/minimal/minimal-main | diff %S/../Output/minimal.out -
RUN: readelf -lW %binary_dir/tests/minimal/minimal-main | FileCheck --check-prefix=SEGMENTS %s
*/

// Check that readelf shows exactly one executable segment
// SEGMENTS-COUNT-1: LOAD{{.*}}R E
// SEGMENTS-NOT:     LOAD{{.*}}R E

#pragma once

// This function does nothing
// LINKARGS: --wrap=foo
void foo();

// This returns an integer
// LINKARGS: --wrap=return_val
int return_val();

// This takes an integer
// LINKARGS: --wrap=arg1
void arg1(int x);
