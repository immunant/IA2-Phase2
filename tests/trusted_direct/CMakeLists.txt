# Build the plugin lib
define_shared_lib(
    SRCS plugin.c
    INCLUDE_DIR include/main
    NEEDS_LD_WRAP
)

# Build the test
define_test(
    SRCS main.c
    INCLUDE_DIR include/plugin
    NEEDS_LD_WRAP
    CRITERION_TEST
)
