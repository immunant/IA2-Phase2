/*
RUN: sh -c 'if [ ! -s "global_fn_ptr_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
RUN: %binary_dir/tests/global_fn_ptr/global_fn_ptr_main_wrapped | diff %S/Output/operations.out -
*/
#include "operations.h"
#include <ia2.h>

uint32_t add(uint32_t x, uint32_t y) { return x + y; }
uint16_t sub(uint16_t x, uint16_t y) { return x - y; }
uint32_t mul(uint32_t x, uint32_t y) { return x * y; }

INIT_RUNTIME(1);
INIT_COMPARTMENT(1);

// `sum` can't be set to __ia2_add since it's defined in asm so the compiler doesn't know it's a valid initializer
//static WordFn sum = __ia2_add;
// REWRITER: static WordFn sum = IA2_FN(add);
static WordFn sum = add;
// REWRITER: static HalfFn diff = IA2_FN(sub);
static HalfFn diff = sub;
// The following won't compile since the inner pointer is also type-specific
//static HalfFn mul = {&__ia2_mul_0_1};

Op operations[2] IA2_SHARED_DATA = {
    {
        { "add", 4 },
        // REWRITER: IA2_FN(add),
        add,
        { "Adds two u32s", 14 },
        0,
    },
    {
        { "mul", 4 },
        // REWRITER: IA2_FN(mul),
        mul,
        { "Multiply two u32s", 18 },
        0,
    },
};

int main() {
    call_operations();
}
