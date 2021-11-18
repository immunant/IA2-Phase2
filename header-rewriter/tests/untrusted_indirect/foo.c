#include <signal.h>
#include "foo.h"
#include "stdio.h"
#include "untrusted_segfault_handler.h"

uint64_t pick_lhs(uint64_t x, uint64_t y) {
    return x;
}

static callback_t function = pick_lhs;
static uint64_t last_result = 0;

bool register_callback(callback_t cb) {
    if (!cb) {
        return false;
    }
    function = cb;
    return true;
}

uint64_t apply_callback(uint64_t x, uint64_t y) {
    last_result = function(x, y);
    return last_result;
}

void unregister_callback() {
    function = pick_lhs;
    if (last_result) {
        uint64_t stolen_secret = *(uint64_t *)last_result;
        printf("UNTRUSTED: the secret is 0x%lx\n", stolen_secret);
    }
}
