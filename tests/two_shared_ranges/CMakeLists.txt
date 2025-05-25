# Build the plugin lib
define_shared_lib(
    SRCS plugin.c
    PKEY 2
    INCLUDE_DIR include/main
    NEEDS_LD_WRAP
)

# Build the test
define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
    INCLUDE_DIR include/plugin
    CRITERION_TEST
)
