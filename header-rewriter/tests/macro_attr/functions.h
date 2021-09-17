/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h --
RUN: cat %t.h | FileCheck %s
*/
#define ATTR __attribute__((hot))

/* CHECK: IA2_WRAP_FUNCTION(f); */
void f();
/* CHECK: IA2_WRAP_FUNCTION(g); */
ATTR void g();
