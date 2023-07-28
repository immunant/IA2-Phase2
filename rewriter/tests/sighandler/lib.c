/*
RUN: cat sighandler_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include "lib.h"
#include <signal.h>
#include <ia2.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

int lib_secret = 4;

// LINKARGS: --wrap=test_handler_from_lib
void test_handler_from_lib(void) {
    raise(SIGTRAP);
}
