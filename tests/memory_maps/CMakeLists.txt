define_shared_lib(
  LIBNAME 2
  SRCS 2.c
  PKEY 2
)

define_shared_lib(
  LIBNAME 3
  SRCS 3.c
  PKEY 3
)

define_shared_lib(
  LIBNAME 4
  SRCS 4.c
  PKEY 4
)

define_test(
  SRCS main.c
  PKEY 1
  LIBS 2 3 4
  NEEDS_LD_WRAP
  CRITERION_TEST
)
