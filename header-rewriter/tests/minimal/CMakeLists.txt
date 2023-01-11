# Build the wrapper lib
define_ia2_wrapper(
    WRAPPER wrapper
    WRAPPED_LIB original
    SRCS main.c
    CALLER_PKEY 1
)

# Build the wrapped lib
define_shared_lib(
    SRCS minimal.c
    LIBNAME original
)

# Build the test
define_test(
    SRCS main.c
    WRAPPERS wrapper
)
