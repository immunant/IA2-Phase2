#pragma once
#include <assert.h>
#include <ia2.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Tests should include this header without defining the following macro to avoid rewriting function
 * pointers that shouldn't be rewritten.
 */
#if !defined(IA2_TEST_RUNNER_SOURCE)
typedef void *ia2_test_fn;
#else
typedef void (*ia2_test_fn)(void);
#endif

struct fake_criterion_test {
  struct fake_criterion_test *next;
  const char *suite;
  const char *name;
  ia2_test_fn test;
  ia2_test_fn init;
  int exit_code;
  int signal;
};

extern struct fake_criterion_test *fake_criterion_tests;

#define STRINGIFY(a) #a
#define EXPAND_AND_STRINGIFY(a) STRINGIFY(a)

/*
 * Placing IA2_{BEGIN,END}_NO_WRAP between the function declaration stops the rewriter from creating a
 * direct call gate for the test function or indirect call gates for function pointer expressions
 * that reference it like the RHS when initializing struct fake_criterion_test's test field. The
 * last line of this macro is the start of the test function's definition and should be followed by { }
 */
#define Test(suite_, name_, ...)                                                            \
  IA2_BEGIN_NO_WRAP                                                                         \
  void fake_criterion_##suite_##_##name_(void);                                             \
  IA2_END_NO_WRAP                                                                           \
  struct fake_criterion_test fake_criterion_##suite_##_##name_##_##test IA2_SHARED_DATA = { \
      .next = NULL,                                                                         \
      .suite = EXPAND_AND_STRINGIFY(suite_),                                                           \
      .name = EXPAND_AND_STRINGIFY(name_),                                                             \
      .test = fake_criterion_##suite_##_##name_,                                            \
      ##__VA_ARGS__};                                                                       \
  __attribute__((constructor)) void fake_criterion_add_##suite_##_##name_##_##test(void) {  \
    fake_criterion_##suite_##_##name_##_##test.next = fake_criterion_tests;                 \
    fake_criterion_tests = &fake_criterion_##suite_##_##name_##_##test;                     \
  }                                                                                         \
  void fake_criterion_##suite_##_##name_(void)

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
 * Configure the signal handler to expect an mpk violation when `expr` is evaluated. If `expr`
 * doesn't trigger a fault, the process exits with a non-zero exit status.
 */
#define CHECK_VIOLATION(expr)                                   \
  ({                                                            \
    expect_fault = true;                                        \
    asm volatile("" : : : "memory");                            \
    volatile typeof(expr) _tmp = expr;                          \
    printf("CHECK_VIOLATION: did not seg fault as expected\n"); \
    _exit(1);                                                   \
    _tmp;                                                       \
  })

extern bool expect_fault;
