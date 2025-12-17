# Test to demonstrate that _dl_debug_state inherits compartment from libc

# Build the library compartment
define_shared_lib(
    SRCS library.c
    NEEDS_LD_WRAP
    PKEY 2
    EXTRA_REWRITER_ARGS --libc-compartment --extra-arg -I${CMAKE_SOURCE_DIR}/tests/common
)

# Test helper: LD_PRELOAD shim to record malloc pkeys.
add_library(dl_debug_malloc_probe SHARED malloc_probe.c)
target_include_directories(dl_debug_malloc_probe PRIVATE ${CMAKE_SOURCE_DIR}/runtime/libia2/include)
target_link_libraries(dl_debug_malloc_probe PRIVATE dl)

# Build the main test
define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    PKEY 1
    CRITERION_TEST
    UNWRAPPED_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/tests/common
    EXTRA_REWRITER_ARGS --libc-compartment --extra-arg -I${CMAKE_SOURCE_DIR}/tests/common
)

# Register only the test cases that actually exist in main.c
foreach(test_case IN ITEMS
    libc_compartment_inheritance
    basic_compartment_check
    dlopen_allocation_compartment
    dlopen_alloc_link_map_pkey
    dlerror_alloc_pkey
    dlopen_allocation_smaps_growth)
    add_test(
        NAME dl_debug_test_${test_case}
        COMMAND ${CMAKE_COMMAND} -E env IA2_TEST_NAME=${test_case} $<TARGET_FILE:dl_debug_test>
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    set_tests_properties(dl_debug_test_${test_case} PROPERTIES LABELS "libc_compartment")
    # Use the malloc probe to capture loader allocations; harmless if weak symbols don't bind.
    set_tests_properties(dl_debug_test_${test_case} PROPERTIES ENVIRONMENT "LD_PRELOAD=$<TARGET_FILE:dl_debug_malloc_probe>")
endforeach()

# Static verification: ensure autowrap generates All loader wrappers.
if(IA2_LIBC_COMPARTMENT)
    add_test(
        NAME dl_debug_test_autowrap_symbols
        COMMAND ${CMAKE_COMMAND}
            -DCALLGATES_SO=$<TARGET_FILE:dl_debug_test_call_gates>
            -P ${CMAKE_SOURCE_DIR}/cmake/verify_autowrap_symbols.cmake
    )
    set_tests_properties(dl_debug_test_autowrap_symbols PROPERTIES LABELS "libc_compartment")
endif()
