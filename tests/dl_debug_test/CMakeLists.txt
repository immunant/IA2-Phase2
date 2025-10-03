# Test to demonstrate that _dl_debug_state inherits compartment from libc

# Build the library compartment
define_shared_lib(
    SRCS library.c
    NEEDS_LD_WRAP
    PKEY 2
)

# Build the main test
define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    PKEY 1
    CRITERION_TEST
)
