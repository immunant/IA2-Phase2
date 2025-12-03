#include "library.h"

#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
  ia2_register_compartment("libdl_debug_getpwnam_lib.so", 2, NULL);
}

Test(dl_debug_getpwnam, loads_nss_backend) {
  cr_assert_eq(trigger_getpwnam_load(), 0);
  cr_assert(ia2_got_pw);
}
