/*
RUN: cp %S/include/liboption.h %S/include/types.h %S/include/impl.h .
RUN: ia2-header-rewriter %T/wrapper.c liboption.h types.h -- -I. -I%resource_dir
RUN: cat liboption.h | sed 's/^.*CHECK.*$//' | FileCheck %S/include/liboption.h
RUN: cat impl.h | sed 's/^.*CHECK.*$//' | FileCheck %S/include/impl.h
*/
#include "liboption.h"
#include "types.h"
#include <stdio.h>
#include <ia2.h>

INIT_COMPARTMENT;

int main() {
    Option x = Some(3);
    Option none = None();
    printf("`x` has value %d\n", unwrap_or(x, -1));
    printf("`none` has no value %d\n", unwrap_or(none, -1));
}
