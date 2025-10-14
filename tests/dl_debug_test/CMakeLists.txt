# Test to demonstrate that _dl_debug_state inherits compartment from libc

# Build the library compartment
define_shared_lib(
    SRCS library.c
    NEEDS_LD_WRAP
    PKEY 2
    EXTRA_REWRITER_ARGS --libc-compartment
)

define_shared_lib(
    LIBNAME fault_plugin
    SRCS plugin_fault.c
    NEEDS_LD_WRAP
    PKEY 3
)

# Build the main test
# fault_plugin is linked so the rewriter can generate call gates, but the test dlopens
# a copied DSO (`libfault_plugin_dlopen.so`) to exercise the mmap wrapper with a fresh map.
define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    PKEY 1
    CRITERION_TEST
    LIBS dl_debug_test_lib fault_plugin
    EXTRA_REWRITER_ARGS --libc-compartment
)

# Produce a dlopen-only copy of the plugin after the padded library is created
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libfault_plugin_dlopen.so
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fault_plugin>
            ${CMAKE_CURRENT_BINARY_DIR}/libfault_plugin_dlopen.so
    DEPENDS fault_plugin)

add_custom_target(fault_plugin_dlopen_copy
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libfault_plugin_dlopen.so)

add_dependencies(dl_debug_test fault_plugin_dlopen_copy)
