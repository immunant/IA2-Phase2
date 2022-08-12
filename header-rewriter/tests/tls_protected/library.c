#include "library.h"
#include "test_fault_handler.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

thread_local uint32_t lib_secret = 0x1eaf1e55;

void lib_print_main_secret() {
  printf("library: main secret is %x\n", CHECK_VIOLATION(main_secret));
}

void lib_print_lib_secret() {
  printf("library: lib secret is %x\n", lib_secret);
}
