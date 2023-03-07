#include <stdio.h>
#include <stdbool.h>
#include "rand_op.h"
#include "test_fault_handler.h"

// This library either returns a pointer to `add` or to `sub`. One of the functions is static and
// the other is part of the public API to ensure that both types of cross-compartment function
// pointers work.

static bool fn_is_add = true;

uint32_t add(uint32_t x, uint32_t y) {
    return x + y;
}

uint32_t sub(uint32_t x, uint32_t y) {
    return x - y;
}

void swap_function(void) {
    fn_is_add = !fn_is_add;
}

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

static uint32_t *secret_address = NULL;

void leak_secret_address(uint32_t *addr) {
    secret_address = addr;
}

extern bool clean_exit;

static uint32_t steal_secret(uint32_t x, uint32_t y) {
    if (!clean_exit) {
        if (secret_address) {
            printf("the secret is %x\n", CHECK_VIOLATION(*secret_address));
        }
    }
    return 0;
}

function_t get_bad_function(void) {
    function_t f = (function_t){
        .name = "",
        .op = steal_secret,
    };
    return f;
}
