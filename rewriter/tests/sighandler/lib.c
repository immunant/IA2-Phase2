#include "lib.h"
#include <signal.h>
#include <ia2.h>

INIT_COMPARTMENT(2);

int lib_secret = 4;

void test_handler_from_lib(void) {
    raise(SIGTRAP);
}