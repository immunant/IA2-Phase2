# Only public headers are passed directly to header-rewriter
set(PUBLIC_HEADERS liboption.h types.h)
set(PRIVATE_HEADERS impl.h)

# Build the wrapper lib
define_ia2_wrapper(
    HEADERS ${PUBLIC_HEADERS}
    PRIVATE_HEADERS ${PRIVATE_HEADERS}
)

# Build the wrapped lib
define_shared_lib(
    SRCS liboption.c impl.c
)

# Build the test
define_test(
    SRCS main.c
)
