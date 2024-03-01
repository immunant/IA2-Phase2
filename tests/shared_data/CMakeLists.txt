# Build the wrapped lib
define_shared_lib(
    SRCS access_shared.c
)

# Build the test
define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    CRITERION_TEST
)

define_ia2_wrapper()
