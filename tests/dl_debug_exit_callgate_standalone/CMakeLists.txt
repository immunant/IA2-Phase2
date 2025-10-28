# Standalone test that explicitly calls exit(0) to exercise __wrap___cxa_finalize

# Build the shared library (copy of dl_debug_exit_callgate library)
define_shared_lib(
    SRCS library.c
    NEEDS_LD_WRAP
    PKEY 2
)

# Build the standalone test (no Criterion, just calls exit(0))
define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    PKEY 1
)
