if(LIBIA2_INSECURE)
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/recursion.insecure.out)
else()
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/recursion.out)
endif()
configure_file(${STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/recursion.out)

# Build the wrapper lib for the recursion
define_ia2_wrapper(
    HEADERS recursion_dso.h
    COMPARTMENT_PKEY 2
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/dso/
    CALLER_PKEY 1
)

# Build the wrapper lib for the main binary
define_ia2_wrapper(
    WRAP_MAIN
    COMPARTMENT_PKEY 1
    HEADERS recursion_main.h
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/main/
    CALLER_PKEY 2
)

# Build the recursion lib
define_shared_lib(
    SRCS dso.c
    LINK_LIBS recursion-main-wrapper
    LINK_OPTS "-Wl,-T${libia2_BINARY_DIR}/padding.ld"
    INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/
)

# Build the test
define_test(
    SRCS main.c
)
