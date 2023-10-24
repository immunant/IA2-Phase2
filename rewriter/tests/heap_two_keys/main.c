#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <criterion/new/assert.h>
#include <unistd.h>
#include <assert.h>
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
Test(heap_two_keys, 0, .init = trigger_compartment_init) {
}

// Test that the main binary's heap can't be read
Test(heap_two_keys, 1, .init = trigger_compartment_init) {
    uint32_t *x = (uint32_t *)malloc(sizeof(uint32_t));
    if (!x) {
        cr_fatal("Failed to allocate memory on the heap");
    }
    *x = 0x09431233;
    read_from_plugin_expect_fault((uint8_t*)x);
    free(x);
    // This test shouldn't return
    cr_fatal("Should have segfaulted but didn't");
}

// Test that the main binary's heap can't be written to
Test(heap_two_keys, 2, .init = trigger_compartment_init) {
    // This zeroes out the allocated memory
    uint8_t *x = (uint8_t *)calloc(sizeof(uint8_t), 12);
    if (!x) {
        cr_fatal("Failed to allocate memory on the heap");
    }
    write_from_plugin_expect_fault(x, 12);
    free(x);
    // This test shouldn't return
    cr_fatal("Should have segfaulted but didn't");
}

// Test that the main binary's shared data can be read
Test(heap_two_keys, 3, .init = trigger_compartment_init) {
    uint16_t *x = (uint16_t *)shared_malloc(sizeof(uint16_t));
    if (!x) {
        cr_fatal("Failed to allocate memory on the heap");
    }
    *x = 0xffed;
    assert(read_from_plugin((uint8_t*)x) == 0xed);
    shared_free(x);
}

// TODO: Add tests for free, realloc, the plugin's heap and reserving more than
// once from the gigacage (> 2MB in allocations)