if(LIBIA2_INSECURE)
    set(PLUGIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/plugin.insecure.out)
    set(MAIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/main.insecure.out)
else()
    set(PLUGIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/plugin.out)
    set(MAIN_STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/main.out)
endif()
configure_file(${PLUGIN_STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/plugin.out)
configure_file(${MAIN_STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/main.out)

# Build the wrapper lib for the plugin
define_ia2_wrapper(
    HEADERS plugin.h
    COMPARTMENT_PKEY 2
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/
    CALLER_PKEY 1
)

# Build the plugin lib
define_shared_lib(
    SRCS plugin.c
    LINK_OPTS "-Wl,-T${libia2_BINARY_DIR}/padding_lib.ld"
    INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/
)

# Build the test
define_test(
    SRCS main.c
)
