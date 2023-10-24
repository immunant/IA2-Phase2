#include "functions.h"
#include <ia2.h>
#include <criterion/criterion.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

Test(macro_attr, main) {
    f();
    g();
    i();
    j();
    k();
}
