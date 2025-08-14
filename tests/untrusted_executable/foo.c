#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>
#include "foo.h"

int secret = 3;

void foo(void) {
}

void access_main_data(int *x) {
    printf("non-secret value in main .data is %d\n", *x);
}
