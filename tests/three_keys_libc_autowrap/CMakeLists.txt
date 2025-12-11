# Minimal 3-compartment test for multicaller loader autowrap verification.
# With libc-compartment enabled and 3 pkeys (1=libc, 2, 3), the rewriter
# generates suffixed wrappers: __wrap_dlopen_from_2, __wrap_dlopen_from_3, etc.

define_shared_lib(
    LIBNAME lib_a
    SRCS lib_a.c
    PKEY 2
    EXTRA_REWRITER_ARGS --libc-compartment
)

define_shared_lib(
    LIBNAME lib_b
    SRCS lib_b.c
    PKEY 3
    EXTRA_REWRITER_ARGS --libc-compartment
)

define_test(
    SRCS main.c
    LIBS lib_a lib_b
    PKEY 1
    CRITERION_TEST
    EXTRA_REWRITER_ARGS --libc-compartment
)

# Register runtime test
add_test(
    NAME three_keys_libc_autowrap_cross_compartment_calls
    COMMAND ${CMAKE_COMMAND} -E env IA2_TEST_NAME=cross_compartment_calls $<TARGET_FILE:three_keys_libc_autowrap>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
set_tests_properties(three_keys_libc_autowrap_cross_compartment_calls PROPERTIES LABELS "libc_compartment")

# Static verification: ensure multicaller autowrap generates suffixed symbols.
# This test specifically validates the _from_<N> suffix pattern.
if(IA2_LIBC_COMPARTMENT AND NOT LIBIA2_AARCH64)
    add_test(
        NAME three_keys_libc_autowrap_multicaller_symbols
        COMMAND ${CMAKE_COMMAND}
            -DCALLGATES_SO=$<TARGET_FILE:three_keys_libc_autowrap_call_gates>
            -P ${CMAKE_SOURCE_DIR}/tests/dl_debug_test/verify_autowrap_symbols.cmake
    )
    set_tests_properties(three_keys_libc_autowrap_multicaller_symbols PROPERTIES LABELS "libc_compartment")
endif()
