if(LIBIA2_INSECURE)
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/untrusted_indirect.insecure.out)
else()
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/untrusted_indirect.out)
endif()
configure_file(${STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/untrusted_indirect.out)

define_shared_lib(SRCS foo.c)

define_test(
    SRCS main.c
    NEEDS_LD_WRAP
)

define_ia2_wrapper()
