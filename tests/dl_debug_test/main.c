/*
 * Test to demonstrate that _dl_debug_state (ld.so) inherits compartment from
 * libc functions that call it, without needing explicit callgates.
 */

#include <ia2_test_runner.h>
#include <ia2.h>
#include <stdio.h>
#include <unistd.h>
#include "library.h"

// This test uses two protection keys
INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

// on_exit handler to set PKRU=0 before destructors run
// on_exit handlers run BEFORE atexit handlers
static void set_pkru_zero_for_exit(int status, void *arg) {
    (void)status;
    (void)arg;
    __asm__ volatile(
        "xor %%eax, %%eax\n"
        "xor %%ecx, %%ecx\n"
        "xor %%edx, %%edx\n"
        "wrpkru\n"
        ::: "eax", "ecx", "edx"
    );
}

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
    ia2_register_compartment("libdl_debug_test_lib.so", 2, NULL);

    // Register handler to run FIRST during exit (before atexit handlers)
    // on_exit handlers run BEFORE atexit, which runs BEFORE destructors
    // Use IA2_IGNORE to prevent rewriter from wrapping this function pointer
    on_exit(IA2_IGNORE(&set_pkru_zero_for_exit), NULL);
}

// Test that iconv (libc) runs in compartment 1 and _dl_debug_state inherits it
Test(dl_debug, libc_compartment_inheritance) {
    cr_log_info("Main: Starting test in compartment 1");

    // Call library function that uses iconv (triggers _dl_debug_state)
    int result = trigger_iconv_dlopen();

    cr_assert_eq(result, 0);
    cr_log_info("Main: Test complete - iconv conversion succeeded, dl_debug_state inherited compartment 1");

    /* TESTING FIX: Set PKRU=0 before test exits to allow destructors to run */
    __asm__ volatile(
        "xor %%eax, %%eax\n"
        "xor %%ecx, %%ecx\n"
        "xor %%edx, %%edx\n"
        "wrpkru\n"
        ::: "eax", "ecx", "edx"
    );
}

// Simple test to verify compartments are working
Test(dl_debug, basic_compartment_check) {
    cr_log_info("Main: Basic compartment check");

    // Call library function in compartment 2
    test_compartment_boundary();

    cr_log_info("Main: Compartment boundaries verified");
}