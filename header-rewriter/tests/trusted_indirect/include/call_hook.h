#pragma once
#include <stdint.h>

typedef uint16_t (*F)(uint16_t *addr);

struct Function {
    F fn;
};

void set_default(F f);
struct Function get_fn();
