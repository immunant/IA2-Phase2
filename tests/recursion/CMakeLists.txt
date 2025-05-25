# Build the recursion lib
define_shared_lib(
    SRCS dso.c
    PKEY 2
    NEEDS_LD_WRAP
    INCLUDE_DIR include/main
)

# Build the test
define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
    INCLUDE_DIR include/dso
    CRITERION_TEST
)
