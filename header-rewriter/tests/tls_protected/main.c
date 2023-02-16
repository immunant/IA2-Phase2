#include <ia2.h>
#include <library.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"
#include <threads.h>

INIT_RUNTIME(2);
INIT_COMPARTMENT(1);

thread_local uint32_t main_secret = 0xdeadbeef;

// This tests that mpk violations call the signal handler in
// test_fault_handler.h and print the appropriate message if the
// segfault occurred in one of the CHECK_VIOLATION expressions. Passing in any
// argument raises a segfault early to test that a violation outside a
// CHECK_VIOLATION prints a different message.
int main(int argc, char **argv) {
  // Access to thread-local from the same compartment should work.
  printf("main: main secret is %x\n", main_secret);
  lib_print_lib_secret();

  // If we have an argument, test the "main accessing lib" direction;
  // otherwise test the "lib accessing main" direction. Both should
  // exit with an MPK violation.
  bool access_lib_secret = argc > 1;

  // Perform forbidden access.
  if (access_lib_secret) {
    printf("main: lib secret is %x\n", CHECK_VIOLATION(lib_secret));
  } else {
    lib_print_main_secret();
  }
}
