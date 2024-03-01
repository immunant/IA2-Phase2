define_shared_lib(
    SRCS lib.c
)

define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    CRITERION_TEST
)

define_ia2_wrapper()
