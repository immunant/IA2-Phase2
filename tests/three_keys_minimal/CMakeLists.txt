define_shared_lib(
    LIBNAME lib_1
    SRCS lib_1.c
    PKEY 3
)

define_shared_lib(
    LIBNAME lib_2
    SRCS lib_2.c
    PKEY 2
)

define_test(
    SRCS main.c
    # Have to manually specify libraries to link against
    # since define_shared_lib invocations here don't use
    # default LIBNAME
    LIBS lib_1 lib_2
    PKEY 1
    CRITERION_TEST
)
