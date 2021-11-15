if(LIBIA2_INSECURE)
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/should_segfault.insecure.out)
else()
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/should_segfault.out)
endif()
    configure_file(${STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/should_segfault.out)

set(HEADERS print_secret.h)

# Build the wrapper lib
define_ia2_wrapper(
    HEADERS ${HEADERS}
)

# Build the wrapped lib
define_shared_lib(
    SRCS print_secret.c
)

# Build the test
define_test(
    SRCS main.c
)
