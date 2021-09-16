/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%S
RUN: cat %t.h | FileCheck %s
*/
#pragma once

struct X {
    int value;
};

enum Bool {
    false,
    true,
};

/* CHECK: IA2_WRAP_FUNCTION(baz) */
int baz();
