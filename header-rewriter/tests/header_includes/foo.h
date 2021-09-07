/*
XFAIL: *
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%S
RUN: cat %t.h | FileCheck %s
*/
#pragma once
#include "bar.h"
#include "baz.h"

/* CHECK: IA2_WRAP_FUNCTION(foo) */
void foo(int x);

/* CHECK: IA2_WRAP_FUNCTION(foo2) */
int foo2();
