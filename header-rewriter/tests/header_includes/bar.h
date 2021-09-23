/*
XFAIL: *
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%S
RUN: cat %t.h | FileCheck %s
*/
#pragma once
#include <baz.h>

struct Option {
    struct X x;
    enum Bool present;
};

/* CHECK: IA2_WRAP_FUNCTION(bar) */
int bar();
