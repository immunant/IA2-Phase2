# Build the wrapped lib
define_shared_lib(
    SRCS library.c
    PKEY 2
)

# Build the test
define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
    CRITERION_TEST
    WITHOUT_SANDBOX
)

define_ia2_wrapper()
