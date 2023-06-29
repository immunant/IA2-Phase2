if(LIBIA2_INSECURE)
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/read_config.insecure.out)
else()
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/read_config.out)
endif()
configure_file(${STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/read_config.out)

define_shared_lib(
    SRCS plugin.c
    PKEY 2
    NEEDS_LD_WRAP
)

define_test(
    SRCS main.c builtin.c
    PKEY 1
    NEEDS_LD_WRAP
)

define_ia2_wrapper()
