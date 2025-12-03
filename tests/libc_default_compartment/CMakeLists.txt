# Verifies that libraries left in the shared compartment (pkey 0)
# can still call into libc.

# Build the shared library in the default (shared) compartment.
define_shared_lib(
    SRCS library.c
    NEEDS_LD_WRAP
    EXTRA_REWRITER_ARGS --libc-compartment
)

# Build the test that links against the shared library.
define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    CRITERION_TEST
    EXTRA_REWRITER_ARGS --libc-compartment
)
