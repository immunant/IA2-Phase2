pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

define_test(
    SRCS main.c
    LIBS LIBUSB_LIBRARIES
    PKEY 1
)

define_ia2_wrapper(
    ORIGINAL_TARGETS libusb libusb_main
    # WRAPPER libusb_wrapper
    # WRAPPED_LIB usb-1.0
    # HEADERS libusb-1.0/libusb.h
    # USE_SYSTEM_HEADERS
    # CALLER_PKEY 1
)
