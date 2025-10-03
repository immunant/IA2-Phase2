# Test exercising mbstowcs-driven gconv loading under compartmentalization

# Exit policy configuration
# Uncomment one of the following to choose the exit policy:

# Option 1: Callgate mode - uses dedicated exit compartment callgates
# - Requires __cxa_finalize wrapper in runtime/libia2/exit_finalize_callgate.c
# - Uses stack switching to exit compartment (pkey 1, same as libc)
# - Test should pass without segfaults when wrapper is present
# - Test will segfault during exit if wrapper is removed from libia2 CMakeLists.txt
# set(EXIT_MODE "callgate")

# Option 2: Union mode (default) - uses union PKRU for destructors (legacy)
# - Destructors run with union of target compartment + libc permissions
# - No stack switching required, no __cxa_finalize wrapper needed
# - Currently used for this test due to known working behavior with gconv
set(EXIT_MODE "union")

# This test stresses gconv module loading during mbstowcs conversion
#
# Useful debugging flags (set via cmake -D<FLAG>=ON):
# - IA2_DEBUG=ON: adds PKRU assertions at call-gate entry/exit, crashes on mismatch (SIGILL/ud2)
# - IA2_DEBUG_MEMORY=ON: enables ia2_log_memory_maps() for annotated /proc/self/maps output
#
# Useful runtime environment variables:
# - IA2_EXIT_POLICY=callgate|union|auto: override exit policy at runtime
# - IA2_PERMISSIVE=1: log PKRU violations to dmesg instead of killing process
# - IA2_TEST_NAME=<test_name>: run single Criterion test case
#
# To test callgate mode: uncomment EXIT_MODE "callgate" above and verify no segfaults
# To verify wrapper necessity: comment out exit_finalize_callgate.c in libia2 CMakeLists.txt
# and observe segfault during program exit in callgate mode

# Set both compile-time and runtime exit policy
set(EXIT_POLICY "${EXIT_MODE}")

define_shared_lib(
    SRCS library.c
    NEEDS_LD_WRAP
    PKEY 2
    EXTRA_REWRITER_ARGS --exit-mode ${EXIT_MODE}
)

define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    PKEY 1
    CRITERION_TEST
    EXTRA_REWRITER_ARGS --exit-mode ${EXIT_MODE}
)

# Set runtime exit policy for test execution
set_tests_properties(${TEST_NAME} PROPERTIES
    ENVIRONMENT "IA2_EXIT_POLICY=${EXIT_POLICY}"
)
