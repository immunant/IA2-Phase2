/*
RUN: cat macro_attr_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <stdio.h>
#include "functions.h"

// LINKARGS: --wrap=f
void f() {
    printf("Called `f()`\n");
}

// LINKARGS: --wrap=g
void g() {
    printf("Called `g()`\n");
}

// TODO(src_rewriter_wip): this gets --wrap, but i don't think it should
void h(CB cb) {
    printf("Calling `cb(0)` from `h`\n");
    cb(0);
}

// LINKARGS: --wrap=i
void i() {
    printf("Called `i()`\n");
}

// LINKARGS: --wrap=j
void j() {
    printf("Called `j()`\n");
}

// LINKARGS: --wrap=k
void k() {
    printf("Called `k()`\n");
}
