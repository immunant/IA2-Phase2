# Test to demonstrate that _dl_debug_state inherits compartment from libc

# Build the library compartment
define_shared_lib(
    SRCS library.c
    NEEDS_LD_WRAP
    PKEY 2
    EXTRA_REWRITER_ARGS --libc-compartment
)

# Build the main test
define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    PKEY 1
    CRITERION_TEST
    EXTRA_REWRITER_ARGS --libc-compartment
)

# Register only the test cases that actually exist in main.c
foreach(test_case IN ITEMS libc_compartment_inheritance basic_compartment_check)
    add_test(
        NAME dl_debug_test_${test_case}
        COMMAND ${CMAKE_COMMAND} -E env IA2_TEST_NAME=${test_case} $<TARGET_FILE:dl_debug_test>
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    set_tests_properties(dl_debug_test_${test_case} PROPERTIES LABELS "libc_compartment")
endforeach()
