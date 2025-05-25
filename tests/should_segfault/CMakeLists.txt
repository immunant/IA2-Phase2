# Build the wrapped lib
define_shared_lib(
    SRCS print_secret.c
)

# Build the test
define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    CRITERION_TEST
)
