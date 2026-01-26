# Verify minimal loader malloc mappings are tagged with the loader pkey

define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    NO_LIBS
    PKEY 1
    CRITERION_TEST
    EXTRA_REWRITER_ARGS --libc-compartment
)
