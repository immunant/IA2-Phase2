set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/structs.out)
configure_file(${STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/structs.out)

define_shared_lib(
    SRCS structs.c
)

define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    CRITERION_TEST
)
