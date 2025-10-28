#include "library.h"

#include <ia2.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
  ia2_register_compartment("libdl_debug_exit_callgate_standalone_lib.so", 2, NULL);
}

int main(void) {
  // Initialize the library state
  trigger_exit_callgate();

  // Print message so we know we got here
  const char msg[] = "standalone_exit_test: calling exit(0)\n";
  write(STDERR_FILENO, msg, sizeof(msg) - 1);

  // Explicitly call exit(0) to trigger __cxa_finalize
  // This should invoke __wrap___cxa_finalize which will run destructors
  exit(0);

  // Should never reach here
  return 1;
}
