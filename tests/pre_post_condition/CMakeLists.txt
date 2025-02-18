# Build the wrapped lib
define_shared_lib(
    SRCS src.c
    PKEY 2
)

# Build the test
define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
    CRITERION_TEST
)

# Build the wrapper lib
define_ia2_wrapper()
