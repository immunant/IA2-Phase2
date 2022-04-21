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
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/plugin/
    CALLER_PKEY 1
)

# Build the wrapper lib for the main binary
define_ia2_wrapper(
    WRAP_MAIN
    COMPARTMENT_PKEY 1
    HEADERS exported_fn.h
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/main/
    CALLER_PKEY 2
)

# Build the plugin lib
define_shared_lib(
    SRCS plugin.c
    LINK_LIBS two_keys_minimal-main-wrapper
    LINK_OPTS "-Wl,-T${IA2_SHLIB_LINKER_SCRIPT}"
    INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/
)
# Declare the plugin as depending on the shared library linker script
set_target_properties(two_keys_minimal-original PROPERTIES LINK_DEPENDS ${IA2_SHLIB_LINKER_SCRIPT})

# Build the test
define_test(
    SRCS main.c
)
