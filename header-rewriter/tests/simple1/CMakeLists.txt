# Build a wrapper for the main binary
define_ia2_wrapper(
    WRAP_MAIN
    COMPARTMENT_KEY 0
    OUTPUT_HEADER main_fn_ptr_ia2.h
    HEADERS hooks.h hook_ty.h
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/main
)

# Build the shared library
define_shared_lib(
    LIBNAME simple1
    SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1/simple1.c
    LINK_LIBS simple1-main-wrapper
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1 ${CMAKE_CURRENT_BINARY_DIR}
)

# Build a wrapper for the shared library
define_ia2_wrapper(
    WRAPPED_LIB simple1
    HEADERS simple1.h
    OUTPUT_HEADER simple1_fn_ptr_ia2.h
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1
)

# Build the main binary
define_test(
    SRCS main.c
    COMPILE_OPTS -Werror=incompatible-pointer-types
)
