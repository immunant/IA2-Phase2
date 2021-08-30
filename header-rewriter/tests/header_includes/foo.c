#include <stdio.h>
#include "foo.h"

void foo(int x) {
    printf("x is %d\n", x);
}

int foo2() {
    return bar() + baz();
}
