/*
RUN: cat untrusted_indirect_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <ia2_test_runner.h>

#include <signal.h>
#include "foo.h"


extern bool clean_exit;

uint64_t pick_lhs(uint64_t x, uint64_t y) {
    return x;
}

static callback_t function = pick_lhs;
static uint64_t last_result = 0;

// LINKARGS: --wrap=apply_callback
// Applies a binary operation to the args using either a registered callback or an internal default function.
uint64_t apply_callback(uint64_t x, uint64_t y) {
    last_result = function(x, y);
    return last_result;
}

// LINKARGS: --wrap=register_callback
bool register_callback(callback_t cb) {
    if (!cb) {
        return false;
    }
    function = cb;
    return true;
}

// LINKARGS: --wrap=unregister_callback
void unregister_callback() {
    function = pick_lhs;
    if (last_result) {
        if (!clean_exit) {
            // Check for an mpk violation when the library tries to read the main binary's memory
            uint64_t stolen_secret = CHECK_VIOLATION(*(uint64_t *)last_result);
            cr_fatal("Did not segfault on boundary violation");
        }
    }
}
