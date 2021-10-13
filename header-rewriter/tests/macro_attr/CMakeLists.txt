set(IA2_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/../include)
set(ORIGINAL_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(REWRITTEN_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Use the directory as the test name
get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(ORIGINAL_LIB ${TEST_NAME}-original)
set(WRAPPER ${TEST_NAME}-wrapper)
set(MAIN ${TEST_NAME}-main)

set(HEADERS functions.h)
set(HEADER_SRCS ${HEADERS})
list(TRANSFORM HEADER_SRCS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
list(TRANSFORM HEADERS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)

add_library(${ORIGINAL_LIB} SHARED functions.c)
target_include_directories(${ORIGINAL_LIB} BEFORE PRIVATE ${ORIGINAL_HEADER_DIR})

add_custom_command(
    OUTPUT ${HEADERS} wrapper.c
    COMMAND cp ${HEADER_SRCS} ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ia2-header-rewriter
      --output-header ${CMAKE_CURRENT_BINARY_DIR}/fn_ptr_ia2.h
      ${CMAKE_CURRENT_BINARY_DIR}/wrapper.c
      ${HEADERS}
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
target_include_directories(${MAIN} BEFORE PRIVATE ${IA2_INCLUDE_DIR} ${REWRITTEN_HEADER_DIR})

add_dependencies(check-ia2 ${MAIN})
