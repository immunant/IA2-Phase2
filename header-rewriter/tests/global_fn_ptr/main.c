#define IA2_INIT_COMPARTMENT 1
#include <ia2.h>

#include "operations.h"

uint32_t add(uint32_t x, uint32_t y) { return x + y; }
uint16_t sub(uint16_t x, uint16_t y) { return x - y; }
uint32_t mul(uint32_t x, uint32_t y) { return x * y; }

INIT_RUNTIME(1);

IA2_DEFINE_WRAPPER(add, _ZTSPFjjjE, 1);
IA2_DEFINE_WRAPPER(sub, _ZTSPFtttE, 1);
IA2_DEFINE_WRAPPER(mul, _ZTSPFjjjE, 1);

// `sum` can't be set to __ia2_add since it's defined in asm so the compiler doesn't know it's a valid initializer
//static WordFn sum = __ia2_add;
static WordFn sum = IA2_WRAPPER(add, 1);
static HalfFn diff = IA2_WRAPPER(sub, 1);
// The following won't compile since the inner pointer is also type-specific
//static HalfFn mul = {&__ia2_mul_0_1};

Op operations[2] IA2_SHARED_DATA = {
    {
        { "add", 4 },
        IA2_WRAPPER(add, 1),
        { "Adds two u32s", 14 },
        0,
    },
    {
        { "mul", 4 },
        IA2_WRAPPER(mul, 1),
        { "Multiply two u32s", 18 },
        0,
    },
};

int main() {
    call_operations();
}
