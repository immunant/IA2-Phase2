/*
XFAIL: *
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%T
RUN: cat %t.h | FileCheck %s
*/
#pragma once
#include <types.h>
#include <impl.h>

/* CHECK: IA2_WRAP_FUNCTION(Some) */
Option Some(int x);

/* CHECK: IA2_WRAP_FUNCTION(None) */
Option None();
