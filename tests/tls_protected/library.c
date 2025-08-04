/*
 */

// Check that readelf shows exactly one executable segment
#include "library.h"

#include <ia2_test_runner.h>

#include <ia2.h>
#include <stdbool.h>
#include <stdlib.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

thread_local uint32_t lib_secret = 0x1eaf1e55;

void lib_print_main_secret() {
  cr_log_info("library: going to access main secret\n");
  cr_log_info("library: accessing main secret at %p\n", &main_secret);
  cr_log_info("library: main secret is %x\n", CHECK_VIOLATION(main_secret));
  cr_assert(false); // should not reach here
}

void lib_print_lib_secret() {
  cr_log_info("library: lib secret is %x\n", lib_secret);
}
