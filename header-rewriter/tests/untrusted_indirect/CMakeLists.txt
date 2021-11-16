define_shared_lib(
    SRCS foo.c
)

define_ia2_wrapper(
    HEADERS foo.h
)

define_test(
    SRCS main.c
)
