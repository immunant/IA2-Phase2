if(LIBIA2_INSECURE)
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/trusted_indirect.insecure.out)
else()
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/trusted_indirect.out)
endif()
configure_file(${STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/trusted_indirect.out)

define_shared_lib(
    SRCS rand_op.c
)

define_ia2_wrapper(
    HEADERS rand_op.h
    CALLER_PKEY 0
)

define_test(
    SRCS main.c
)
