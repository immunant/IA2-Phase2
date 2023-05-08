/*
RUN: cat sighandler_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include "lib.h"
#include <signal.h>
#include <ia2.h>

INIT_COMPARTMENT(2);

int lib_secret = 4;

// LINKARGS: --wrap=test_handler_from_lib
void test_handler_from_lib(void) {
    raise(SIGTRAP);
}
