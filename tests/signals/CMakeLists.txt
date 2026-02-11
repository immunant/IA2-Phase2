define_shared_lib(
  SRCS lib.c
  PKEY 2
)

define_test(
  SRCS main.c
  PKEY 1
  NEEDS_LD_WRAP
  CRITERION_TEST
)
