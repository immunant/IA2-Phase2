# Build the wrapped lib
define_shared_lib(
    SRCS library.c
)

# Build the test
define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
    CRITERION_TEST
    WITHOUT_SANDBOX
)
