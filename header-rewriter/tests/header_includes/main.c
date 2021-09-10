#include "foo.h"
#include "bar.h"
#include "baz.h"
#include <stdio.h>

int main() {
    struct Option opt_x = Some(3);
    if (opt_x.present) {
        printf("opt_x has value %d\n", opt_x.x.value);
    }
    printf("%d\n", foo2());
    printf("%d\n", bar());
    printf("%d\n", baz());
}
