# Build the wrapper lib
define_ia2_wrapper(
    HEADERS operations.h
    CALLER_PKEY 1
)

# Build the wrapped lib
define_shared_lib(
    SRCS operations.c
)

# Build the test
define_test(
    SRCS main.c
)
