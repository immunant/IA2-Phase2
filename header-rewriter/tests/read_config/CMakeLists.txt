define_ia2_wrapper(
    WRAP_MAIN
    COMPARTMENT_PKEY 1
    CALLER_PKEY 2
    HEADERS core.h
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/
    OUTPUT_DIR main_shim
)

define_ia2_wrapper(
    COMPARTMENT_PKEY 2
    CALLER_PKEY 1
    HEADERS plugin.h core.h
    SHARED_HEADERS core.h
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/
    OUTPUT_DIR plugin_shim
)

define_shared_lib(
    SRCS plugin.c
    INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/main_shim
    LINK_LIBS read_config-main-wrapper
    LINK_OPTS "-Wl,-T${libia2_BINARY_DIR}/padding_lib.ld"
)

define_test(
    SRCS main.c builtin.c
    COMPILE_OPTS -Werror=incompatible-pointer-types
    INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/plugin_shim
    LINK_LIBS read_config-wrapper
)
