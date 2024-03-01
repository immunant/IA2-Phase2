define_shared_lib(
    SRCS plugin.c
    PKEY 2
    NEEDS_LD_WRAP
)

define_test(
    SRCS main.c builtin.c
    PKEY 1
    NEEDS_LD_WRAP
    CRITERION_TEST
)

define_ia2_wrapper()
