if(LIBIA2_INSECURE)
    set(MAIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/tls_protected_main.insecure.out)
    set(LIB_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/tls_protected_lib.insecure.out)
else()
    set(MAIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/tls_protected_main.out)
    set(LIB_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/tls_protected_lib.out)
endif()
configure_file(${MAIN_STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR})
configure_file(${LIB_STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR})

set(HEADERS library.h)

# Build the wrapper lib
define_ia2_wrapper(
    HEADERS ${HEADERS}
    CALLER_PKEY 1
)

# Build the wrapped lib
define_shared_lib(
    SRCS library.c
)

# Build the test
define_test(
    SRCS main.c
)
