/*
RUN: sh -c 'if [ ! -s "shared_data_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/
#include <stdint.h>
#include <assert.h>
#include <ia2.h>
#include "access_shared.h"
#include <ia2_test_runner.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
}

uint8_t shared_val[4097] IA2_SHARED_DATA = { 0 };

void check_shared_access(uint8_t *shared) {
    uint8_t original = *shared;
    uint8_t result = read_shared(shared);
    cr_assert(original == *shared);
    cr_assert(original == result);
    uint8_t old_val = write_shared(shared, original + 1);
    cr_assert(original == old_val);
}

Test(shared_data, main) {
    shared_val[0] = 23;
    shared_val[4096] = 254;
    check_shared_access(&shared_val[0]);
    check_shared_access(&shared_val[4096]);
}
