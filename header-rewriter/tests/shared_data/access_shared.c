/*
RUN: cat shared_data_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <stdio.h>
#include "access_shared.h"

// LINKARGS: --wrap=read_shared
void read_shared(uint8_t *shared) {
    printf("read %d from shared variable\n", *shared);
}

// LINKARGS: --wrap=write_shared
uint8_t write_shared(uint8_t *shared, uint8_t new_value) {
    uint8_t old_value = *shared;
    printf("writing %d to shared variable\n", new_value);
    *shared = new_value;
    return old_value;
}
