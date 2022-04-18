# Build the wrapper lib
define_ia2_wrapper(
    WRAPPER function_allowlist_wrapper
    WRAPPED_LIB function_allowlist_original
    HEADERS library.h
    CALLER_PKEY 1
    EXTRA_REWRITER_ARGS --function-allowlist=library.h.fns
)

# Build the wrapped lib
define_shared_lib(
    SRCS library.c
    LIBNAME function_allowlist_original
)

# Build the test
define_test(
    SRCS main.c
    WRAPPERS function_allowlist_wrapper
)
