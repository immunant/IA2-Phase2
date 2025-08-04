/*
RUN: sh -c 'if [ ! -s "dav1d_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/

// Check that readelf shows exactly one executable segment

#include "dav1d.h"
#include <ia2.h>
#include <ia2_test_runner.h>
#include <signal.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
  ia2_register_compartment("libpost_condition_lib.so", 2, NULL);
}

Dav1dContext c IA2_SHARED_DATA;
Dav1dPicture pic IA2_SHARED_DATA;

Test(post_condition, normal) {
  corrupt_stride = false;
  dav1d_get_picture(&c, &pic);
}

Test(post_condition, corrupt_bounds, .signal = SIGABRT) {
  corrupt_stride = true;
  dav1d_get_picture(&c, &pic);
}
