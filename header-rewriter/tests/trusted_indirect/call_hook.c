#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "call_hook.h"

static F default_fn = NULL;

void set_default(F f) {
    default_fn = f;
}

static uint16_t increment(uint16_t *addr) {
    return *addr;
}

struct Function get_fn() {
    if (!default_fn) {
        return (struct Function){
            .fn = &increment
        };
    }
    return (struct Function){
        .fn = default_fn
    };
}
