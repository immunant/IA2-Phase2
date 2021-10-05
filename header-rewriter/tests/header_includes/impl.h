/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%T
RUN: cat %t.h | FileCheck %s
*/
#pragma once
#include <types.h>

/* CHECK: IA2_WRAP_FUNCTION(unwrap_or) */
int unwrap_or(Option opt, int default_value);
