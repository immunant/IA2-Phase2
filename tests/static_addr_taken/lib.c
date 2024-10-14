#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <ia2.h>

//#include <test_fault_handler.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

#include "static_fns.h"

static void duplicate_noop(void) {
    printf("called %s in library\n", __func__);
}

static void identical_name(void) {
    static int x = 4;
    printf("%s in library read x = %d\n", __func__, x);
}

static fn_ptr_ty ptrs[3] IA2_SHARED_DATA = {
    inline_noop, duplicate_noop, identical_name
};

fn_ptr_ty *get_ptrs_in_lib(void) {
    return ptrs;
}

Test(static_addr_taken, call_ptrs_in_lib) {
    for (int i = 0; i < 3; i++) {
        ptrs[i]();
    }
}

Test(static_addr_taken, call_ptr_from_main) {
    fn_ptr_ty *main_ptrs = get_ptrs_in_main();
    for (int i = 0; i < 3; i++) {
        main_ptrs[i]();
    }
}
