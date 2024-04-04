#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * This header defines a test framework for detecting MPK violations using
 * signal handlers. This file must be included exactly once from a source file
 * in the main binary with IA2_DEFINE_TEST_HANDLER defined by the preprocessor.
 * This will define the functions and variables used by the test handler, ensure
 * it is initialized before main and provide access to the LOG and
 * CHECK_VIOLATION macros. Other files which need CHECK_VIOLATION or LOG may
 * include the header without defining IA2_DEFINE_TEST_HANDLER. Using
 * CHECK_VIOLATION without defining the test handler will trigger a linker error
 * when building the shared object.
 */

#define VA_ARGS(...) , ##__VA_ARGS__
#define LOG(msg, ...) printf("%s: " msg "\n", __func__ VA_ARGS(__VA_ARGS__))

// Configure the signal handler to expect an mpk violation when `expr` is
// evaluated. If `expr` doesn't trigger a fault, this macro manually raises a
// fault with a different message.
#define CHECK_VIOLATION(expr)                                                  \
  ({                                                                           \
    expect_fault = true;                                                       \
    asm volatile("" : : : "memory");                                           \
    volatile typeof(expr) _tmp = expr;                                         \
    printf("CHECK_VIOLATION: did not seg fault as expected\n");                \
    _exit(0);                                                                  \
    _tmp;                                                                      \
  })

#ifndef IA2_DEFINE_TEST_HANDLER
extern bool expect_fault;
#else
// This is shared data to allow checking for violations in multiple
// compartments. We avoid using IA2_SHARED_DATA here to avoid including ia2.h
// since that would pull in libia2 as a dependency (the libia2 build generates a
// header included in ia2.h).
bool expect_fault __attribute__((section("ia2_shared_data"))) = false;

// Create a stack for the signal handler to use
char sighandler_stack[4 * 1024] __attribute__((section("ia2_shared_data")))
__attribute__((aligned(16))) = {0};
char *sighandler_sp __attribute__((section("ia2_shared_data"))) =
    &sighandler_stack[(4 * 1024) - 8];

// This function must be declared naked because it's not necessarily safe for it
// to write to the stack in its prelude (the stack isn't written to when the
// function itself is called because it's only invoked as a signal handler).
#if LIBIA2_X86_64
__attribute__((naked)) void handle_segfault(int sig) {
  // This asm must preserve %rdi which contains the argument since
  // print_mpk_message reads it
  __asm__(
      // Signal handlers are defined in the main binary, but they don't run with
      // the same pkru state as the interrupted context. This means we have to
      // remove all MPK restrictions to ensure can run it correctly.
      "xorl %ecx, %ecx\n"
      "xorl %edx, %edx\n"
      "xorl %eax, %eax\n"
      "wrpkru\n"
      // Switch the stack to a shared buffer. There's only one u32 argument and
      // no returns so we don't need a full wrapper here.
      "movq sighandler_sp@GOTPCREL(%rip), %rsp\n"
      "movq (%rsp), %rsp\n"
      "callq print_mpk_message");
}
#elif LIBIA2_AARCH64
#warning "Review test_fault_handler implementation after enabling x18 switching"
void print_mpk_message(int sig);
void handle_segfault(int sig) {
    print_mpk_message(sig);
}
#endif

// The test output should be checked to see that the segfault occurred at the
// expected place.
void print_mpk_message(int sig) {
  if (sig == SIGSEGV) {
    // Write directly to stdout since printf is not async-signal-safe
    const char *ok_msg = "CHECK_VIOLATION: seg faulted as expected\n";
    const char *early_fault_msg = "CHECK_VIOLATION: unexpected seg fault\n";
    const char *msg;
    if (expect_fault) {
      msg = ok_msg;
    } else {
      msg = early_fault_msg;
    }
    write(1, msg, strlen(msg));
    if (!expect_fault) {
      _exit(-1);
    }
  }
  _exit(0);
}

// Installs the previously defined signal handler and disables buffering on
// stdout to allow using printf prior to the sighandler
__attribute__((constructor)) void install_segfault_handler(void) {
  setbuf(stdout, NULL);
  signal(SIGSEGV, handle_segfault);
}
#endif
