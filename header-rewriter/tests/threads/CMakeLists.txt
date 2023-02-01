if(LIBIA2_INSECURE)
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/threads.insecure.out)
else()
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/threads.out)
endif()
configure_file(${STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/threads.out)

# Build the wrapper lib
define_ia2_wrapper(
    WRAPPER threads_wrapper
    WRAPPED_LIB threads_original
    HEADERS library.h
    CALLER_PKEY 1
)

# Build the wrapped lib
define_shared_lib(
    SRCS library.c
    LIBNAME threads_original
)

# Build the test
define_test(
    SRCS main.c
    WRAPPERS threads_wrapper
)
