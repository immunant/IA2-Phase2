# Use the directory as the test name
get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(ORIGINAL_LIB ${TEST_NAME}-original)
set(WRAPPER ${TEST_NAME}-wrapper)

set(HEADERS random.hpp)

# Build the wrapper lib
define_ia2_wrapper(
    WRAPPER ${WRAPPER}
    WRAPPED_LIB ${ORIGINAL_LIB}
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    HEADERS ${HEADERS}
)

# Build the original lib
define_shared_lib(
    SRCS random.cpp
)

# Build the test
define_test(
    SRCS main.cpp
)
