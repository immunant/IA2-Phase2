#include <stdio.h>
#include <foo.h>

struct Option Some(int x) {
    printf("x is %d\n", x);
    struct Option opt = {
        .x = x,
        .present = true,
    };
    return opt;
}

int foo2() {
    return bar() + baz();
}
