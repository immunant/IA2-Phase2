#pragma once

#define ATTR __attribute__((hot))
#define UNUSED __attribute__((unused))
#define EMPTY
#define EMPTY_FNLIKE(x)
#define EMPTY_VARIADIC_FNLIKE(...)

void f();
ATTR void g();

// CHECK: typedef struct IA2_fnptr__ZTSPFiiE CB;
UNUSED typedef int (*CB)(int);

void h(CB cb);

EMPTY void i();

EMPTY_FNLIKE(0)
void j();

EMPTY_VARIADIC_FNLIKE(1, 2)
void k();
