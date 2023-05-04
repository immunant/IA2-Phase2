if(LIBIA2_INSECURE)
    set(MAIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/tls_protected_main.insecure.out)
    set(LIB_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/tls_protected_lib.insecure.out)
else()
    set(MAIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/tls_protected_main.out)
    set(LIB_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/tls_protected_lib.out)
endif()
configure_file(${MAIN_STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/tls_protected_main.out)
configure_file(${LIB_STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/tls_protected_lib.out)

set(HEADERS library.h)

# Build the wrapped lib
define_shared_lib(
    SRCS library.c
    PKEY 2
)

# Build the test
define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
)

define_ia2_wrapper()
