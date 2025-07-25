# Build the lib
define_shared_lib(
    SRCS foo.c
    PKEY 1
)

# Build the test
define_test(
    SRCS main.c
    PKEY 0
    CRITERION_TEST
)
