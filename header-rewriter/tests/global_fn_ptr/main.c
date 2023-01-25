#include "operations.h"
#include <ia2.h>

uint32_t add(uint32_t x, uint32_t y) { return x + y; }
uint16_t sub(uint16_t x, uint16_t y) { return x - y; }
uint32_t mul(uint32_t x, uint32_t y) { return x * y; }

INIT_RUNTIME(1);
INIT_COMPARTMENT(1);

// `sum` can't be set to __ia2_add since it's defined in asm so the compiler doesn't know it's a valid initializer
//static WordFn sum = __ia2_add;
static WordFn sum = add;
static HalfFn diff = sub;
// The following won't compile since the inner pointer is also type-specific
//static HalfFn mul = {&__ia2_mul_0_1};

Op operations[2] IA2_SHARED_DATA = {
    {
        { "add", 4 },
        add,
        { "Adds two u32s", 14 },
        0,
    },
    {
        { "mul", 4 },
        mul,
        { "Multiply two u32s", 18 },
        0,
    },
};

int main() {
    call_operations();
}
