#include <criterion/logging.h>
#include <criterion/new/assert.h>

struct fake_criterion_test {
  int (*test)(void);
  int exit_code;
};

#define Test(suite, name, ...)                                                                                                 \
  int fake_criterion_##suite##_##name(void);                                                                                   \
  __attribute__((__section__("fake_criterion_tests"))) struct fake_criterion_test fake_criterion_##suite##_##name##_##test = { \
      .test = fake_criterion_##suite##_##name,                                                                                 \
      ##__VA_ARGS__};                                                                                                          \
  int fake_criterion_##suite##_##name(void)
