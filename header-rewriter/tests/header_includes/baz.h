/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%S
RUN: cat %t.h | FileCheck %s
*/
#pragma once

/* CHECK: IA2_WRAP_FUNCTION(baz) */
int baz();
