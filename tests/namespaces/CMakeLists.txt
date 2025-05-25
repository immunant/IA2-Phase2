set(HEADERS random.hpp)

# Build the original lib
define_shared_lib(
    SRCS random.cpp
)

# Build the test
define_test(
    SRCS main.cpp
)
