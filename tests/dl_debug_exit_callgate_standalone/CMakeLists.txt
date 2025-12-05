# Standalone test that explicitly calls exit(0) to exercise __wrap___cxa_finalize

# Build the shared library (copy of dl_debug_exit_callgate library)
define_shared_lib(
    SRCS library.c
    NEEDS_LD_WRAP
    PKEY 2
    EXTRA_REWRITER_ARGS --libc-compartment
)

# Build the standalone test (no Criterion, just calls exit(0))
define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    PKEY 1
    EXTRA_REWRITER_ARGS --libc-compartment
)

add_test(
    NAME dl_debug_exit_callgate_standalone
    COMMAND $<TARGET_FILE:dl_debug_exit_callgate_standalone>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
set_tests_properties(dl_debug_exit_callgate_standalone PROPERTIES LABELS "libc_compartment")
