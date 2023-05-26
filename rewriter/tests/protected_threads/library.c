/*
RUN: cat threads_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include "library.h"
#include <ia2.h>
#include <stdio.h>
#include <stdlib.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

void lib_nop(void) {
}
