define_shared_lib(SRCS rand_op.c)

define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    CRITERION_TEST
)
