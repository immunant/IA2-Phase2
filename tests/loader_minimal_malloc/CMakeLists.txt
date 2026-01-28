# Verify minimal loader malloc mappings are tagged with the loader pkey
# Note: This test requires IA2_LIBC_COMPARTMENT (which implies custom glibc build)

define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    NO_LIBS
    PKEY 1
    CRITERION_TEST
    UNWRAPPED_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/external/glibc/elf
    EXTRA_REWRITER_ARGS --libc-compartment
)
