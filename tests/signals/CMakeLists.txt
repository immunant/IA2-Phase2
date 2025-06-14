define_shared_lib(
  SRCS lib.c
  PKEY 2
)

define_test(
  SRCS main.c
  PKEY 1
  NEEDS_LD_WRAP
  CRITERION_TEST
  WITHOUT_SANDBOX # tracer can't handle unexpected signals yet (see #488)
)
