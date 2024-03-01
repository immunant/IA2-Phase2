define_shared_lib(
    SRCS simple1.c
    NEEDS_LD_WRAP
    PKEY 0
)

define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    PKEY 1
    CRITERION_TEST
)

define_ia2_wrapper()

