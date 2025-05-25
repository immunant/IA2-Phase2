pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

define_test(
    SRCS main.c
    LIBS LIBUSB_LIBRARIES
    PKEY 1
)
