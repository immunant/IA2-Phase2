/*
RUN: sh -c 'if [ ! -s "untrusted_indirect_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/
#include <ia2_test_runner.h>

#include <stdint.h>
#include "foo.h"
#include <ia2.h>
#define IA2_DEFINE_TEST_HANDLER


INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

/*
    This program tests that a trusted binary can pass function pointers to an untrusted shared
    library. Function pointers passed between compartments must be wrapped in call gates to ensure
    that code from each compartment is executed with the appropriate pkru state. Function pointers
    must be wrapped by manually rewriting them as IA2_FNPTR_WRAPPER(ptr). This produces an opaque
    pointer which can then be passed across compartments. An MPK violation is triggered by the
    untrusted library if no arguments are passed in, otherwise the program exits cleanly.
*/

// This test required aligning and padding the segments in the mpk-protected binary using the
// `padding.ld` script. To stress test the script let's define some variables, sections and
// functions that will be placed in various segments.

// Declare a read-write variable, read-only variable and uninitialized variable in custom sections.
uint32_t initialized_var __attribute__((section("my_var_section"))) __attribute__((used)) = 0x11223344;
const uint32_t immutable_var __attribute__((section("my_const_var_section"))) __attribute__((used)) = 0x55667788;
uint32_t uninit_var __attribute__((section("my_uninit_var_section"))) __attribute__((used));

bool clean_exit IA2_SHARED_DATA = false;

// Declare some new sections with different flags.
__asm__(".section my_alloc_section, \"a\"\n\
    .byte 0\n\
    .previous");

__asm__(".section my_write_section, \"w\"\n\
    .byte 0\n\
    .previous");

__asm__(".section my_executable_section, \"x\"\n\
    .byte 0\n\
    .previous");

// Place a function in a custom section.
__attribute__((section("my_fn_section"))) uint64_t pick_rhs(uint64_t x, uint64_t y) {
    return y;
}

static uint64_t secret = 0xcafed00d;

uint64_t leak_secret_address(uint64_t x, uint64_t y) {
    return (uint64_t)&secret;
}

void do_test() {
    cr_log_info("TRUSTED: the secret is 0x%lx\n", secret);
    cr_log_info("0x%lx\n", apply_callback(1, 2));

    // REWRITER: register_callback(IA2_FN(pick_rhs));
    register_callback(pick_rhs);
    cr_log_info("0x%lx\n", apply_callback(3, 4));

    // REWRITER: register_callback(IA2_FN(leak_secret_address));
    register_callback(leak_secret_address);
    cr_log_info("TRUSTED: oops we leaked the address of the secret\n");
    apply_callback(5, 6);

   unregister_callback();
}

Test(untrusted_indirect, no_clean_exit) {
    clean_exit = false;
    do_test();
}

Test(untrusted_indirect, clean_exit) {
    clean_exit = true;
    do_test();
}