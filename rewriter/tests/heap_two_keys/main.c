/*
RUN: sh -c 'if [ ! -s "heap_two_keys_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
RUN: sh -c 'if [ ! -s "heap_two_keys_call_gates_2.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
TODO: %binary_dir/tests/heap_two_keys/heap_two_keys_main_wrapped 0 | diff %binary_dir/tests/heap_two_keys/plugin.out -
// TODO(src_rewriter_wip): had to change the output here, why?
RUN: %binary_dir/tests/heap_two_keys/heap_two_keys_main_wrapped 1 | diff %binary_dir/tests/heap_two_keys/main.out -
TODO: %binary_dir/tests/heap_two_keys/heap_two_keys_main_wrapped 2 | diff %source_dir/tests/heap_two_keys/Output/clean_exit.out -
*/
#include <stdio.h>
#include <unistd.h>
#include <ia2.h>
#include <ia2_allocator.h>
#include "plugin.h"
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

// This test uses two protection keys
INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

// Test that the program can exit without error
// TODO(#112): it cannot.
int test_0() {
    return 0;
}

// Test that the main binary's heap can't be read
int test_1() {
    uint32_t *x = (uint32_t *)malloc(sizeof(uint32_t));
    if (!x) {
        LOG("Failed to allocate memory on the heap");
        return -1;
    }
    *x = 0x09431233;
    read_uint32_t_expect_fault(x);
    free(x);
    // This test shouldn't return
    return -1;
}

// Test that the main binary's heap can't be written to
int test_2() {
    // This zeroes out the allocated memory
    uint8_t *x = (uint8_t *)calloc(sizeof(uint8_t), 12);
    if (!x) {
        LOG("Failed to allocate memory on the heap");
        return -1;
    }
    write_uint8_t_expect_fault(x, 12);
    free(x);
    // This test shouldn't return
    return -1;
}

// Test that the main binary's shared data can be read
int test_3() {
    uint16_t *x = (uint16_t *)shared_malloc(sizeof(uint16_t));
    if (!x) {
        LOG("Failed to allocate memory on the heap");
        return -1;
    }
    *x = 0xffed;
    assert(read_uint16_t(x) == 0xffed);
    shared_free(x);
    return 0;
}

// TODO: Add tests for free, realloc, the plugin's heap and reserving more than
// once from the gigacage (> 2MB in allocations)

int main(int argc, char **argv) {
    if (argc < 2) {
        LOG("Run with an integer (0-3) as the first argument to select a test");
        return -1;
    }

    // Call a no-op function to switch to this compartment's PKRU
    trigger_compartment_init();

    int test_num = *argv[1] - '0';

    switch (test_num) {
        case 0: {
            return test_0();
        }
        case 1: {
            return test_1();
        }
        case 2: {
            return test_2();
        }
        case 3: {
            return test_3();
        }
        default: {
            LOG("Unknown test selected");
            return -1;
        }
    }
}
