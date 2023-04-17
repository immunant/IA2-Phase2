define_shared_lib(
    SRCS lib.c
)

define_test(
    SRCS main.c
    NEEDS_LD_WRAP
)

define_ia2_wrapper()
