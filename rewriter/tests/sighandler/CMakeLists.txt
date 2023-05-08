if(LIBIA2_INSECURE)
    set(MAIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/main.insecure.out)
else()
    set(MAIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/main.out)
endif()
configure_file(${MAIN_STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/main.out)

# Build the plugin lib
define_shared_lib(
    SRCS lib.c
    PKEY 2
)

# Build the test
define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    PKEY 1
)

define_ia2_wrapper()
