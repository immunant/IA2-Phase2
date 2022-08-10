/*
RUN: cp %S/include/liboption.h %S/include/types.h %S/include/impl.h .
RUN: ia2-header-rewriter %T/wrapper.c liboption.h types.h -- -I. -I%resource_dir
RUN: cat %T/wrapper.c.args | FileCheck --check-prefix=LINKARGS %S/include/liboption.h
RUN: cat %T/wrapper.c.args | FileCheck --check-prefix=LINKARGS %S/include/impl.h
*/
#include "liboption.h"
#include "types.h"
#include <stdio.h>
#include <ia2.h>

INIT_RUNTIME(1);
INIT_COMPARTMENT(1);

int main() {
    Option x = Some(3);
    Option none = None();
    printf("`x` has value %d\n", unwrap_or(x, -1));
    printf("`none` has no value %d\n", unwrap_or(none, -1));
}
