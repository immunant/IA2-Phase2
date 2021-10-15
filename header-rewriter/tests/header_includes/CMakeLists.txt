set(ORIGINAL_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/)
set(REWRITTEN_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Use the directory as the test name
get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(ORIGINAL_LIB ${TEST_NAME}-original)
set(WRAPPER ${TEST_NAME}-wrapper)
set(MAIN ${TEST_NAME}-main)

# Only public headers are passed directly to header-rewriter
set(PUBLIC_HEADERS liboption.h types.h)
set(PRIVATE_HEADERS impl.h)
set(HEADER_SRCS ${PUBLIC_HEADERS} ${PRIVATE_HEADERS})
list(TRANSFORM HEADER_SRCS PREPEND ${ORIGINAL_HEADER_DIR}/)
list(TRANSFORM PUBLIC_HEADERS PREPEND ${REWRITTEN_HEADER_DIR}/)

add_library(${ORIGINAL_LIB} SHARED liboption.c impl.c)
target_include_directories(${ORIGINAL_LIB} BEFORE PRIVATE ${ORIGINAL_HEADER_DIR})

add_custom_command(
    OUTPUT ${PUBLIC_HEADERS} ${PRIVATE_HEADERS} wrapper.c
    COMMAND cp ${HEADER_SRCS} ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ia2-header-rewriter
      ${CMAKE_CURRENT_BINARY_DIR}/wrapper.c
      ${PUBLIC_HEADERS}
      --
      -I ${REWRITTEN_HEADER_DIR}
      -I ${C_SYSTEM_INCLUDE}
    DEPENDS ${HEADER_SRCS}
    VERBATIM)

add_library(${WRAPPER} SHARED wrapper.c)
target_link_libraries(${WRAPPER} PRIVATE
    -Wl,--version-script,${CMAKE_CURRENT_BINARY_DIR}/wrapper.c.syms
    ${ORIGINAL_LIB})
target_include_directories(${WRAPPER} BEFORE PRIVATE ${IA2_INCLUDE_DIR})

add_executable(${MAIN} main.c)
target_link_libraries(${MAIN} PRIVATE
    ${ORIGINAL_LIB} ${WRAPPER})
target_include_directories(${MAIN} BEFORE PRIVATE
    ${IA2_INCLUDE_DIR}
    ${REWRITTEN_HEADER_DIR})

add_dependencies(check-ia2 ${MAIN})

