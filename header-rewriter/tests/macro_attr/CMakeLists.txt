# Use the directory as the test name
get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(ORIGINAL_LIB ${TEST_NAME}-original)
set(WRAPPER ${TEST_NAME}-wrapper)

set(HEADERS functions.h)

# Build the wrapper lib
define_ia2_wrapper(
    WRAPPER ${WRAPPER}
    WRAPPED_LIB ${ORIGINAL_LIB}
    HEADERS ${HEADERS}
)

# Build the wrapped lib
define_shared_lib(
    SRCS functions.c
)

# Build the test
define_test(
    SRCS main.c
)
