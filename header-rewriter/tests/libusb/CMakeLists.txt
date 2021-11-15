# Build the wrapper lib
define_ia2_wrapper(
    WRAPPER libusb_wrapper
    WRAPPED_LIB usb-1.0
    HEADERS libusb-1.0/libusb.h
    USE_SYSTEM_HEADERS
)

# Build the test
define_test(
    SRCS main.c
    WRAPPERS libusb_wrapper
)
