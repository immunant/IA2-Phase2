#include "library.h"

#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
}

Test(libc_default_compartment, library_stays_in_pkey0) {
  int result = call_libc_from_pkey0();
  cr_assert_eq(result, 0);
}
