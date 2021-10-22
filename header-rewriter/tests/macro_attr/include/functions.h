/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h --
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
*/
#define ATTR __attribute__((hot))
#define UNUSED __attribute__((unused))
#define EMPTY
#define EMPTY_FNLIKE(x)
#define EMPTY_VARIADIC_FNLIKE(...)

// CHECK: IA2_WRAP_FUNCTION(f);
void f();
// CHECK: IA2_WRAP_FUNCTION(g);
ATTR void g();

// CHECK: typedef struct IA2_fnptr__ZTSPFiiE CB;
UNUSED typedef int (*CB)(int);

void h(CB cb);

// CHECK: IA2_WRAP_FUNCTION(i);
EMPTY void i();

// CHECK: IA2_WRAP_FUNCTION(j);
EMPTY_FNLIKE(0) void j();

// CHECK: IA2_WRAP_FUNCTION(k);
EMPTY_VARIADIC_FNLIKE(1, 2) void k();
