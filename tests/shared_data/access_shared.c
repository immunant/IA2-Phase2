/*
RUN: cat shared_data_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <stdio.h>
#include "access_shared.h"
#include <ia2_test_runner.h>


// LINKARGS: --wrap=read_shared
uint8_t read_shared(uint8_t *shared) {
    cr_log_info("read %d from shared variable\n", *shared);
    return *shared;
}

// LINKARGS: --wrap=write_shared
uint8_t write_shared(uint8_t *shared, uint8_t new_value) {
    uint8_t old_value = *shared;
    cr_log_info("writing %d to shared variable\n", new_value);
    *shared = new_value;
    return old_value;
}
