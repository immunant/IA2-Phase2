/*
XFAIL: *
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%T
RUN: cat %t.h | FileCheck %s
*/
#pragma once

typedef enum Bool {
    false,
    true,
} bool;

typedef struct Option {
    int value;
    bool present;
} Option;
