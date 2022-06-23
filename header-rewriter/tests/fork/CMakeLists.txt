# Build the wrapper lib
define_ia2_wrapper(
    WRAPPER fork_wrapper
    WRAPPED_LIB fork_original
    HEADERS library.h
    CALLER_PKEY 1
)

# Build the wrapped lib
define_shared_lib(
    SRCS library.c
    LIBNAME fork_original
)

# Build the test
define_test(
    SRCS main.c
    WRAPPERS fork_wrapper
)
