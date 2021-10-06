/*
XFAIL: *
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%T
RUN: cat %t.h | FileCheck %s
*/
#pragma once
#include <stdbool.h>

typedef struct Option {
    int value;
    bool present;
} Option;
