# Build the plugin lib
define_shared_lib(
    SRCS lib.c
    PKEY 2
)

# Build the test
define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    PKEY 1
)

define_ia2_wrapper()
