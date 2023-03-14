#include "recursion_main.h"
#include <ia2.h>
#include <stdio.h>

INIT_COMPARTMENT(2);

void recurse_dso(int count) {
    printf("recursion_dso: %d\n", count);
    if (count > 0) {
        recurse_main(count - 1);
    }
}
