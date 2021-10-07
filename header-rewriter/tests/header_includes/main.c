/*
RUN: cp %S/liboption.h %S/types.h %S/impl.h .
RUN: ia2-header-rewriter wrapper.c liboption.h types.h -- -I. -I%resource_dir
RUN: cat liboption.h | FileCheck %S/liboption.h
RUN: cat impl.h | FileCheck %S/impl.h
*/
#include <liboption.h>
#include <types.h>
#include <stdio.h>

int main() {
    Option x = Some(3);
    Option none = None();
    printf("`x` has value %d\n", unwrap_or(x, -1));
    printf("`none` has no value %d\n", unwrap_or(none, -1));
}
