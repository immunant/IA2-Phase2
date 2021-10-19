# Use the directory as the test name
get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(WRAPPER ${TEST_NAME}-wrapper)
set(MAIN ${TEST_NAME}-main)

# Build the wrapper lib
define_ia2_wrapper(
    WRAPPER libusb_wrapper
    WRAPPED_LIB usb-1.0
    HEADERS libusb.h
)

# Build the test
add_executable(${MAIN} main.c)
target_link_libraries(${MAIN} PRIVATE
    libusb_wrapper)

add_dependencies(check-ia2 ${MAIN})

