if(LIBIA2_INSECURE)
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/recursion.insecure.out)
else()
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/recursion.out)
endif()
configure_file(${STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/recursion.out)

# Build the recursion lib
define_shared_lib(
    SRCS dso.c
    PKEY 2
    NEEDS_LD_WRAP
    INCLUDE_DIR include/main
)

# Build the test
define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
    INCLUDE_DIR include/dso
)

define_ia2_wrapper()
