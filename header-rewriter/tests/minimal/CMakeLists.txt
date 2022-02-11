# Build the wrapper lib
define_ia2_wrapper(
    WRAPPER wrapper
    WRAPPED_LIB original
    HEADERS minimal.h
    CALLER_KEY 0
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
