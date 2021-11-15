# Use the directory as the test name
get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(ORIGINAL_LIB ${TEST_NAME}-original)
set(WRAPPER ${TEST_NAME}-wrapper)

# Only public headers are passed directly to header-rewriter
set(PUBLIC_HEADERS liboption.h types.h)
set(PRIVATE_HEADERS impl.h)

# Build the wrapper lib
define_ia2_wrapper(
    WRAPPER ${WRAPPER}
    WRAPPED_LIB ${ORIGINAL_LIB}
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
