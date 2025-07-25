#include <ia2_test_runner.h>
#include <ia2.h>
#include "foo.h"

void ia2_main(void) {
    ia2_register_compartment("libfoo.so", 1, NULL);
}

INIT_RUNTIME(1);

Test(untrusted_executable, direct_call_lib) {
    foo();
}
