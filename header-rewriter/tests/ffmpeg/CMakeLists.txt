# Wrappers are defined in cmake/common-wrappers.cmake

# Build the test
define_test(
    SRCS main.c
    WRAPPERS avformat_wrapper avutil_wrapper
)
