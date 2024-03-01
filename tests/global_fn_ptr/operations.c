/*
RUN: cat global_fn_ptr_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include "operations.h"
#include <criterion/criterion.h>
#include <stdio.h>

extern Op operations[2];

// LINKARGS: --wrap=call_operation
uint32_t call_operation(size_t i) {
    // TODO: Add a way to share strings between compartments
    //printf("%s\n", operations[i].desc.data);
    uint32_t x = 18923;
    uint32_t y = 24389;
    uint32_t res = operations[i].function(x, y);
    return res;
}
