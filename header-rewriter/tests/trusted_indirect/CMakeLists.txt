define_shared_lib(
    SRCS call_hook.c
)

define_ia2_wrapper(
    HEADERS call_hook.h
    OUTPUT_HEADER trusted_indirect_ia2.h
)

define_test(
    SRCS main.c
)
