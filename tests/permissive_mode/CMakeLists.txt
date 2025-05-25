define_test(
    SRCS permissive_mode.c
    NEEDS_LD_WRAP
    PKEY 1
    NO_LIBS
    CRITERION_TEST
    WITHOUT_SANDBOX
)
