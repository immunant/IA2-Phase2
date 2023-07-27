/*
RUN: readelf -lW %binary_dir/tests/tls_protected/libtls_protected_lib_wrapped.so | FileCheck --check-prefix=SEGMENTS %s
*/

// Check that readelf shows exactly one executable segment
// SEGMENTS-COUNT-1: LOAD{{.*}}R E
// SEGMENTS-NOT:     LOAD{{.*}}R E
#include "library.h"
#include "test_fault_handler.h"
#include <ia2.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

thread_local uint32_t lib_secret = 0x1eaf1e55;

void lib_print_main_secret() {
  printf("library: going to access main secret\n");
  printf("library: accessing main secret at %p\n", &main_secret);
  printf("library: main secret is %x\n", CHECK_VIOLATION(main_secret));
}

void lib_print_lib_secret() {
  printf("library: lib secret is %x\n", lib_secret);
}
