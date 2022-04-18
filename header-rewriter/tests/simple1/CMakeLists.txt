# Create a shim for calling simple1-main from libsimple1.so
define_ia2_wrapper(
    WRAP_MAIN
    # libsimple1.so wraps function pointers so we need the macros from the
    # output header
    OUTPUT_HEADER
    # simple1-main has pkey 1
    COMPARTMENT_PKEY 1
    # libsimple1.so has pkey 0 (untrusted)
    CALLER_PKEY 0
    HEADERS hooks.h
    # Where to find the input headers
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1
    # Put the output headers in BIN_DIR/main_shim since hooks.h is also passed
    # to the rewriter for the other shim
    OUTPUT_DIR main_shim
)

define_shared_lib(
    LIBNAME simple1
    SRCS simple1.c
    # Use the main shim's headers to make sure calls to simple1-main are wrapped
    INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/main_shim
    # Link against the first shim to wrap calls to main
    LINK_LIBS simple1-main-wrapper
)

# Create a shim for calling libsimple1.so from simple1-main
define_ia2_wrapper(
    WRAPPED_LIB simple1
    # simple1-main wraps function pointers so we need the macros from the output
    # header
    OUTPUT_HEADER simple1_ia2.h
    # simple1.h defines functions we need to generate wrappers for.
    # hooks.h defines a function pointer type void(*)(void) we need to generate
    # macros for.
    HEADERS simple1.h hooks.h
    # Where to find the input headers
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1
    # Put the output haeders in BIN_DIR/lib_shim since hooks.h it's function
    # declarations are not rewritten for this shim.
    OUTPUT_DIR lib_shim
    # simple1-main has pkey 1
    # COMPARTMENT_PKEY defaults to 0 (untrusted) since libsimple1.so has no pkey
    CALLER_PKEY 1
    # hooks.h also has the functions defined outside this compartment which we should
    # not generate wrappers for so we must blacklist it.
    HEADER_BLACKLIST hooks.h
)

define_test(
    SRCS main.c
    COMPILE_OPTS -Werror=incompatible-pointer-types
    # Use the lib shim's headers to make sure calls to libsimple1.so are wrapped
    # and that we have access to the function pointers macros for the type
    # defined in hooks.h
    INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/lib_shim
)
