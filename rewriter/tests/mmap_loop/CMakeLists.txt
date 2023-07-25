set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/mmap_loop.out)
configure_file(${STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/mmap_loop.out)

define_shared_lib(
    SRCS mmap_loop.c
    PKEY 2
)

define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
)

define_ia2_wrapper()
