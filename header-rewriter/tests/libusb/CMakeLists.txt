set(IA2_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/../include)
set(ORIGINAL_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/)
set(REWRITTEN_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/)

# Use the directory as the test name
get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(WRAPPER ${TEST_NAME}-wrapper)
set(MAIN ${TEST_NAME}-main)

set(HEADERS libusb-1.0/libusb.h)
set(HEADER_SRCS ${HEADERS})
list(TRANSFORM HEADER_SRCS PREPEND ${ORIGINAL_HEADER_DIR}/)
list(TRANSFORM HEADERS PREPEND ${REWRITTEN_HEADER_DIR}/)

set(REWRITER_INCLUDES -I ${REWRITTEN_HEADER_DIR} -I /usr/lib/clang/11.1.0/include/)
add_custom_command(
    OUTPUT ${HEADERS} wrapper.c
    COMMAND echo ${HEADER_SRCS}
    COMMAND mkdir -p ${REWRITTEN_HEADER_DIR}/libusb-1.0/
    COMMAND cp ${HEADER_SRCS} ${REWRITTEN_HEADER_DIR}/libusb-1.0/
    COMMAND ia2-header-rewriter ${CMAKE_CURRENT_BINARY_DIR}/wrapper.c ${HEADERS} -- ${REWRITER_INCLUDES}
    DEPENDS ${HEADER_SRCS}
    VERBATIM)

add_library(${WRAPPER} SHARED wrapper.c)
target_link_libraries(${WRAPPER} PRIVATE
    -Wl,--version-script,wrapper.c.syms)
target_include_directories(${WRAPPER} BEFORE PRIVATE ${IA2_INCLUDE_DIR} ${REWRITTEN_HEADER_DIR})

add_executable(${MAIN} main.c)
target_link_libraries(${MAIN} PRIVATE
    usb-1.0 ${WRAPPER})
target_include_directories(${MAIN} BEFORE PRIVATE ${IA2_INCLUDE_DIR} ${REWRITTEN_HEADER_DIR})

add_dependencies(check-ia2 ${MAIN})

