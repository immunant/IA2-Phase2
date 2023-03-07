if(LIBIA2_INSECURE)
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/trusted_direct.insecure.out)
else()
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/trusted_direct.out)
endif()
configure_file(${STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/trusted_direct.out)

# Build the plugin lib
define_shared_lib(
    SRCS plugin.c
    INCLUDE_DIR include/main
    NEEDS_LD_WRAP
)

# Build the test
define_test(
    SRCS main.c
    INCLUDE_DIR include/plugin
    NEEDS_LD_WRAP
)

define_ia2_wrapper()
