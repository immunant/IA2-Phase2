# Verify minimal loader malloc mappings are tagged with the loader pkey
# Note: This test is only added on x86_64 with LIBIA2_REBUILD_GLIBC (see tests/CMakeLists.txt)

define_test(
    SRCS main.c
    NEEDS_LD_WRAP
    NO_LIBS
    PKEY 1
    CRITERION_TEST
    UNWRAPPED_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/external/glibc/elf
    EXTRA_REWRITER_ARGS --libc-compartment
)
