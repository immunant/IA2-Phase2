#include <ia2_test_runner.h>
#include <ia2.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

#include "static_fns.h"

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
    ia2_register_compartment("libstatic_addr_taken_lib.so", 2, NULL);
}

static void duplicate_noop(void) {
    printf("called %s in main binary\n", __func__);
}

LOCAL void macro_attr_noop(void) {
    printf("called %s in main binary\n", __func__);
}

// static void identical_name(void) {
//     static int x = 3;
//     printf("%s in main binary read x = %d\n", __func__, x);
// }

static fn_ptr_ty ptrs[] IA2_SHARED_DATA = {
    inline_noop, duplicate_noop, /* identical_name, */ macro_attr_noop,
};

fn_ptr_ty *get_ptrs_in_main(void) {
    return ptrs;
}

Test(static_addr_taken, call_ptrs_in_main) {
    for (int i = 0; i < sizeof(ptrs) / sizeof(ptrs[0]); i++) {
        ptrs[i]();
    }
}

Test(static_addr_taken, call_ptr_from_lib) {
    fn_ptr_ty *lib_ptrs = get_ptrs_in_lib();
    for (int i = 0; i < 3; i++) {
        lib_ptrs[i]();
    }
}
