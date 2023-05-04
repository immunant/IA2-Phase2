/*
RUN: sh -c 'if [ ! -s "tls_protected_call_gates_2.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/

#include "library.h"
#include "test_fault_handler.h"
#include <ia2.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

INIT_COMPARTMENT(2);

thread_local uint32_t lib_secret = 0x1eaf1e55;

void lib_print_main_secret() {
  printf("library: going to access main secret\n");
  printf("library: accessing main secret at %p\n", &main_secret);
  printf("library: main secret is %x\n", CHECK_VIOLATION(main_secret));
}

void lib_print_lib_secret() {
  printf("library: lib secret is %x\n", lib_secret);
}
