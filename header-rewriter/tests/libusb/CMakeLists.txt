# Use the directory as the test name
get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(WRAPPER ${TEST_NAME}-wrapper)


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
