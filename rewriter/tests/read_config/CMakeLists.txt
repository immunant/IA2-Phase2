define_shared_lib(
    SRCS plugin.c
    PKEY 2
    NEEDS_LD_WRAP
    NEEDS_OBJCOPY
)

define_shared_lib(
    SRCS core.c foo.c
    PKEY 3
    NEEDS_LD_WRAP
    NEEDS_OBJCOPY
)

define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
    LIBS read_config_lib
         read_config_lib_pkey_3
)

define_ia2_wrapper(
    ORIGINAL_TARGETS read_config_main
                     read_config_lib
                     read_config_lib_pkey_3
)
