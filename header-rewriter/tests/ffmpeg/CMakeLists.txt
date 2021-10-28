# Use the directory as the test name
get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(WRAPPER ${TEST_NAME}-wrapper)
set(MAIN ${TEST_NAME}-main)

set(AVFORMAT_HEADERS libavformat/avformat.h libavformat/avio.h)
set(AVUTIL_HEADERS libavutil/dict.h libavutil/version.h libavutil/macros.h libavutil/common.h)

set(ORIGINAL_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/)
set(REWRITTEN_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Build wrapper libs
define_ia2_wrapper(
    WRAPPER avformat_wrapper
    WRAPPED_LIB avformat
    HEADERS ${AVFORMAT_HEADERS}
    USE_SYSTEM_HEADERS
)

define_ia2_wrapper(
    WRAPPER avutil_wrapper
    WRAPPED_LIB avutil
    HEADERS ${AVUTIL_HEADERS}
    USE_SYSTEM_HEADERS
)

# Build the test
add_executable(${MAIN} main.c)
target_link_libraries(${MAIN} PRIVATE
     avformat_wrapper avutil_wrapper)

add_dependencies(check-ia2 ${MAIN})

