#include <ia2_test_runner.h>
#include <ia2.h>
#include "foo.h"

static int not_secret = 4;

void ia2_main(void) {
    ia2_register_compartment("libuntrusted_executable_lib.so", 1, NULL);
}

INIT_RUNTIME(1);

Test(untrusted_executable, direct_call_lib) {
    foo();
}

Test(untrusted_executable, access_protected_mem) {
    CHECK_VIOLATION(secret += 1);
}

Test(untrusted_executable, access_unprotected_mem) {
    access_main_data(&not_secret);
}
