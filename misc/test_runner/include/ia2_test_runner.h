#pragma once
#include <stdio.h>

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
