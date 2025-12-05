define_shared_lib(
    SRCS library.c
    NEEDS_LD_WRAP
    PKEY 2
    EXTRA_REWRITER_ARGS --libc-compartment
)

define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    PKEY 1
    CRITERION_TEST
    EXTRA_REWRITER_ARGS --libc-compartment
)

set_tests_properties(dl_debug_getpwnam PROPERTIES LABELS "libc_compartment")
