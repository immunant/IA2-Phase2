if(LIBIA2_INSECURE)
    set(PLUGIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/plugin.insecure.out)
    set(MAIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/main.insecure.out)
else()
    set(PLUGIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/plugin.out)
    set(MAIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/main.out)
endif()
configure_file(${PLUGIN_STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/plugin.out)
configure_file(${MAIN_STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/main.out)

# Build the plugin lib
define_shared_lib(
    SRCS plugin.c
    PKEY 2
    INCLUDE_DIR include/main
)

# Build the test
define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
    INCLUDE_DIR include/plugin
)

define_ia2_wrapper()
