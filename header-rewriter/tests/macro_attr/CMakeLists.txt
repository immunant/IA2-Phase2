set(HEADERS functions.h)

# Build the wrapper lib
define_ia2_wrapper(
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
