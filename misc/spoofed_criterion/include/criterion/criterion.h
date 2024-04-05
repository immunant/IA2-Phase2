#include <criterion/logging.h>
#include <criterion/new/assert.h>

#define Test(suite, name) \
    int fake_criterion_##suite##_##name(void); \
    __attribute__((__section__("fake_criterion_tests"))) \
        int (*fake_criterion_##suite##_##name##_##test)(void) = fake_criterion_##suite##_##name; \
    int fake_criterion_##suite##_##name(void)
