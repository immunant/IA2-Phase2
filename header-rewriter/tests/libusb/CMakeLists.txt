wrap_system_lib(
    LIBNAME libusb
    WRAPPED_LIB libusb-1.0
    PKEY 0
    HEADERS libusb.h
)

define_test(
    SRCS main.c
    LIBS libusb
    PKEY 1
)

define_ia2_wrapper(
    ORIGINAL_TARGETS libusb libusb_main
    # WRAPPER libusb_wrapper
    # USE_SYSTEM_HEADERS
    # CALLER_PKEY 1
)
