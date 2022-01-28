if(LIBIA2_INSECURE)
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/trusted_direct.insecure.out)
else()
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/trusted_direct.out)
endif()
configure_file(${STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/trusted_direct.out)

# Build the wrapper lib for the plugin
define_ia2_wrapper(
    HEADERS plugin.h
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/plugin/
)

# Build the wrapper lib for the main binary
define_ia2_wrapper(
    WRAP_MAIN
    HEADERS exported_fn.h
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/main/
)

# Build the plugin lib
define_shared_lib(
    SRCS plugin.c
    LINK_LIBS trusted_direct-main-wrapper
    INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/
)

# Build the test
define_test(
    SRCS main.c
)
