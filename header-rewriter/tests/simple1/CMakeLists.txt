define_ia2_wrapper(
    WRAP_MAIN
    OUTPUT_HEADER simple1_ia2.h
    HEADERS hooks.h
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1
)

define_shared_lib(
    LIBNAME simple1
    SRCS libsimple1/simple1.c
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1
    WRAPPERS simple1-main-wrapper
)

define_ia2_wrapper(
    WRAPPED_LIB simple1
    HEADERS simple1.h
    PRIVATE_HEADERS hooks.h
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1
)

define_test(
    SRCS main.c
    COMPILE_OPTS -Werror=incompatible-pointer-types
)
