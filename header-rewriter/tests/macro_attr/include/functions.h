/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h --
RUN: cat %t.c.args | FileCheck --check-prefix=LINKARGS %s
*/
#define ATTR __attribute__((hot))
#define UNUSED __attribute__((unused))
#define EMPTY
#define EMPTY_FNLIKE(x)
#define EMPTY_VARIADIC_FNLIKE(...)

// LINKARGS: --wrap=f
void f();
// LINKARGS: --wrap=g
ATTR void g();

// CHECK: typedef struct IA2_fnptr__ZTSPFiiE CB;
UNUSED typedef int (*CB)(int);

void h(CB cb);

// LINKARGS: --wrap=i
EMPTY void i();

// LINKARGS: --wrap=j
EMPTY_FNLIKE(0) void j();

// LINKARGS: --wrap=k
EMPTY_VARIADIC_FNLIKE(1, 2) void k();
