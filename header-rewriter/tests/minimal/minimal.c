#include "minimal.h"
#include <stdio.h>

void foo() {
    printf("foo\n");
}

int return_val() {
    printf("return_val\n");
    return 1;
}

void arg1(int x) {
    printf("arg1\n");
}
