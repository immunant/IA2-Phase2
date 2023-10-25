define_shared_lib(
    SRCS mmap_loop.c
    PKEY 2
)

define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
    CRITERION_TEST
)

define_ia2_wrapper()
