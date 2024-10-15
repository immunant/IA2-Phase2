#pragma once
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

struct fake_criterion_test {
  void (*test)(void);
  void (*init)(void);
  int exit_code;
};

#define Test(suite, name, ...)                                                                                                 \
  void fake_criterion_##suite##_##name(void);                                                                                  \
  __attribute__((__section__("fake_criterion_tests"))) struct fake_criterion_test fake_criterion_##suite##_##name##_##test = { \
      .test = fake_criterion_##suite##_##name,                                                                                 \
      ##__VA_ARGS__};                                                                                                          \
  void fake_criterion_##suite##_##name(void)

#define cr_log_info(f, ...) printf(f "\n", ##__VA_ARGS__)
#define cr_log_error(f, ...) fprintf(stderr, f "\n", ##__VA_ARGS__)

#define cr_assert assert
#define cr_assert_eq(a, b) cr_assert((a) == (b))
#define cr_assert_lt(a, b) cr_assert((a) < (b))
#define cr_fatal(s)          \
  do {                       \
    fprintf(stderr, s "\n"); \
    exit(1);                 \
  } while (0)

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

// Configure the signal handler to expect an mpk violation when `expr` is
// evaluated. If `expr` doesn't trigger a fault, this macro manually raises a
// fault with a different message.
#define CHECK_VIOLATION(expr)                                                  \
  ({                                                                           \
    expect_fault = true;                                                       \
    asm volatile("" : : : "memory");                                           \
    volatile typeof(expr) _tmp = expr;                                         \
    printf("CHECK_VIOLATION: did not seg fault as expected\n");                \
    _exit(1);                                                                  \
    _tmp;                                                                      \
  })

extern bool expect_fault;
