#include <ia2_test_runner.h>

#include <ia2.h>
#include <library.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"
#include <threads.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

thread_local uint32_t main_secret = 0xdeadbeef;

volatile void *addr;

// This tests that mpk violations call the signal handler in
// test_fault_handler.h and print the appropriate message if the
// segfault occurred in one of the CHECK_VIOLATION expressions. Passing in any
// argument raises a segfault early to test that a violation outside a
// CHECK_VIOLATION prints a different message.
void run_test(bool access_lib_secret) {
  errno = 5;
  cr_log_info("errno=%d, pkru=%08x\n", errno, ia2_get_pkru());

  lib_print_lib_secret();

  // Access to thread-local from the same compartment should work.
  cr_log_info("main: main secret is %x\n", main_secret);
  cr_log_info("errno=%d, pkru=%08x\n", errno, ia2_get_pkru());
  lib_print_lib_secret();

  cr_log_info("errno=%d, pkru=%08x\n", errno, ia2_get_pkru());

  errno = 5;
  cr_log_info("pkru=%08x\n", ia2_get_pkru());
  cr_log_info("errno=%d\n", errno);

  // Perform forbidden access.
  if (access_lib_secret) {
    cr_log_info("main: going to access lib secret\n");
    addr = &lib_secret;
    if (addr != 0) {
      cr_log_info("main: accessing lib secret at %p\n", addr);
    }
    cr_log_info("main: lib secret is %x\n", CHECK_VIOLATION(lib_secret));
    cr_assert(false); // Should not reach here
  } else {
    lib_print_main_secret();
  }
}

Test(tls_protected, no_access_lib_secret) {
  run_test(false);
}

Test(tls_protected, access_lib_secret) {
  run_test(true);
}