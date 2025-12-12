// Minimal 3-compartment test for multicaller loader autowrap verification
#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(3);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

// Declarations for cross-compartment calls
extern void lib_a_noop(void);
extern void lib_b_noop(void);

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
    ia2_register_compartment("liblib_a.so", 2, NULL);
    ia2_register_compartment("liblib_b.so", 3, NULL);
}

Test(three_keys_libc_autowrap, cross_compartment_calls) {
    // Exercise cross-compartment calls to verify wrappers work
    lib_a_noop();
    lib_b_noop();
}
