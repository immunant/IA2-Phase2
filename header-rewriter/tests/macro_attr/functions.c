#include <stdio.h>
#include <functions.h>

void f() {
    printf("Called `f()`\n");
}

void g() {
    printf("Called `g()`\n");
}

void h(int(*cb)(int)) {
    printf("Calling `cb(0)` from `h`\n");
    cb(0);
}
