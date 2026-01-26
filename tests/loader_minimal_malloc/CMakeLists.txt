# Verify minimal loader malloc mappings are tagged with the loader pkey

if (LIBIA2_AARCH64)
    message(STATUS "Skipping loader_minimal_malloc on aarch64")
    return()
endif()

define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    NO_LIBS
    PKEY 1
    CRITERION_TEST
    UNWRAPPED_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/external/glibc/elf
    EXTRA_REWRITER_ARGS --libc-compartment
)
