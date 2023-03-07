define_shared_lib(
    SRCS simple1.c
    INCLUDE_DIR libsimple1
    NEEDS_LD_WRAP
    PKEY 0
)

define_test(
    SRCS main.c
    INCLUDE_DIR libsimple1
    NEEDS_LD_WRAP
    PKEY 1
)

define_ia2_wrapper()

