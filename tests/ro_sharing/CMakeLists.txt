# Build the plugin lib
define_shared_lib(
    SRCS plugin.c
    PKEY 2
)

# Build the test
define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
    CRITERION_TEST
)
