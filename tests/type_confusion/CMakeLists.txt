# Build the wrapped lib
define_shared_lib(
    SRCS dav1d.c
    PKEY 2
)

# Build the test
define_test(
    SRCS main.c
    PKEY 1
    NEEDS_LD_WRAP
    CRITERION_TEST
    WITHOUT_SANDBOX # TODO remove once tracer bug (#488) is fixed
)

# This test uses the type registry, so link it into the call gates
target_link_libraries(type_confusion_call_gates PRIVATE type-registry)
