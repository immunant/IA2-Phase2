#include "library.h"

#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
  ia2_register_compartment("libdl_debug_exit_callgate_lib.so", 2, NULL);
}

Test(dl_debug_exit_callgate, handler_runs_and_touches_libc) {
  trigger_exit_callgate();
}
