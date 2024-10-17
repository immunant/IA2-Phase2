/*
RUN: cat trusted_indirect_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include <ia2_test_runner.h>


#include <stdbool.h>
#include "rand_op.h"


// This library either returns a pointer to `add` or to `sub`. One of the functions is static and
// the other is part of the public API to ensure that both types of cross-compartment function
// pointers work.

extern bool clean_exit;
static bool fn_is_add = true;
static uint32_t *secret_address = NULL;

uint32_t add(uint32_t x, uint32_t y) {
    return x + y;
}

static uint32_t steal_secret(uint32_t x, uint32_t y) {
    if (!clean_exit) {
        if (secret_address) {
            cr_log_info("the secret is %x\n", CHECK_VIOLATION(*secret_address));
            cr_fatal("Should have segfaulted here");
        }
    }
    return 0;
}

// LINKARGS: --wrap=get_bad_function
function_t get_bad_function(void) {
    function_t f = (function_t){
        .name = "",
        .op = steal_secret,
    };
    return f;
}

// LINKARGS: --wrap=get_function
function_t get_function(void) {
    function_t f;
    if (fn_is_add) {
        f.name = "add";
        f.op = add;
    } else {
        f.name = "sub";
        f.op = sub;
    }
    return f;
}

// LINKARGS: --wrap=leak_secret_address
void leak_secret_address(uint32_t *addr) {
    secret_address = addr;
}

// LINKARGS: --wrap=sub
uint32_t sub(uint32_t x, uint32_t y) {
    return x - y;
}

// LINKARGS: --wrap=swap_function
void swap_function(void) {
    fn_is_add = !fn_is_add;
}
