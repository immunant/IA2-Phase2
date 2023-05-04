set(HEADERS random.hpp)

# Build the wrapper lib
define_ia2_wrapper(
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    HEADERS ${HEADERS}
    CALLER_PKEY 1
)

# Build the original lib
define_shared_lib(
    SRCS random.cpp
)

# Build the test
define_test(
    SRCS main.cpp
)
