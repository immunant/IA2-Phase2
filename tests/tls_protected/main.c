#include <ia2_test_runner.h>

#include <ia2.h>
#include <library.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include <threads.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
  ia2_register_compartment("libtls_protected_lib.so", 2, NULL);
}

thread_local uint32_t main_secret = 0xdeadbeef;

volatile void *addr;

// This tests that mpk violations call the signal handler in
// test_fault_handler.h and print the appropriate message if the
// segfault occurred in one of the CHECK_VIOLATION expressions. Passing in any
// argument raises a segfault early to test that a violation outside a
// CHECK_VIOLATION prints a different message.
void run_test(bool access_lib_secret) {
  errno = 5;
  const char *tag_register =
#ifdef __x86_64__
      "pkru";
#else
      "x18";
#endif
  cr_log_info("errno=%d, %s=%08zx\n", errno, tag_register, ia2_get_tag());

  lib_print_lib_secret();

  // Access to thread-local from the same compartment should work.
  cr_log_info("main: main secret is %x\n", main_secret);
  cr_log_info("errno=%d, %s=%08zx\n", errno, tag_register, ia2_get_tag());
  lib_print_lib_secret();

  cr_log_info("errno=%d, %s=%08zx\n", errno, tag_register, ia2_get_tag());

  errno = 5;
  cr_log_info("%s=%08zx\n", tag_register, ia2_get_tag());
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
