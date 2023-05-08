#include "lib.h"
#include <signal.h>
#include <ia2.h>

INIT_COMPARTMENT(2);

void test_handler(void) {
    raise(SIGTRAP);
}
