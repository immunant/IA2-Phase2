#include <stdio.h>
#include "functions.h"

void f() {
    printf("Called `f()`\n");
}

void g() {
    printf("Called `g()`\n");
}

void h(CB cb) {
    printf("Calling `cb(0)` from `h`\n");
    cb(0);
}

void i() {
    printf("Called `i()`\n");
}

void j() {
    printf("Called `j()`\n");
}

void k() {
    printf("Called `k()`\n");
}
