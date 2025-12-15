#include <ia2_test_runner.h>
#include <ia2.h>
#include <ia2_loader.h>
#include <ia2_test_pkey_utils.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "library.h"

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
    ia2_register_compartment("libdl_debug_test_lib.so", 2, NULL);
}

Test(dl_debug, libc_compartment_inheritance) {
    cr_log_info("Main: Starting test in compartment 1");

    int result = trigger_iconv_dlopen();

    cr_assert_eq(result, 0);
    cr_log_info("Main: Test complete - iconv conversion succeeded, dl_debug_state inherited compartment 1");
}

Test(dl_debug, basic_compartment_check) {
    cr_log_info("Main: Basic compartment check");

    test_compartment_boundary();

    cr_log_info("Main: Compartment boundaries verified");
}

// PartitionAlloc: ensure allocations made while the loader gate flag is active
// still land on the expected compartment protection key (pkey 1 here).
Test(dl_debug, loader_allocator_partitionalloc) {
    ia2_loader_gate_enter();
    void *test_alloc = malloc(64);
    ia2_loader_gate_exit();

    cr_assert(test_alloc != NULL);

    int alloc_pkey = ia2_test_get_addr_pkey(test_alloc);
    cr_assert(alloc_pkey == 1);

    free(test_alloc);
}
