#include <criterion/logging.h>

#define Test(suite, name) \
    void suite##_##name(void); \
    __attribute__((__section__("fake_criterion_tests"))) void (*suite##_##name##_##test)(void) = suite##_##name; \
    void suite##_##name(void)
