#include <criterion/logging.h>
#include <criterion/new/assert.h>

#define Test(suite, name) \
    int suite##_##name(void); \
    __attribute__((__section__("fake_criterion_tests"))) int (*suite##_##name##_##test)(void) = suite##_##name; \
    int suite##_##name(void)
