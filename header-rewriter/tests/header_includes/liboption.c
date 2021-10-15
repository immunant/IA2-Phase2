#include <stdio.h>
#include "liboption.h"
#include "types.h"

Option Some(int x) {
    printf("returning `Some(%d)`\n", x);
    Option opt = {
        .value = x,
        .present = true,
    };
    return opt;
}

Option None() {
    printf("returning `None`\n");
    Option none = {
        .value = 0,
        .present = false,
    };
    return none;
}
