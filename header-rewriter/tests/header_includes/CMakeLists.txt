add_library(header_includes-original SHARED foo.c bar.c baz.c)

set(HEADERS foo.h bar.h baz.h)
set(HEADER_SRCS ${HEADERS})
list(TRANSFORM HEADERS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)
list(TRANSFORM HEADER_SRCS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)

add_custom_command(
  OUTPUT ${HEADERS} wrapper.c
  COMMAND cp ${HEADER_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/
  COMMAND ia2-header-rewriter
    ${CMAKE_CURRENT_BINARY_DIR}/wrapper.c
    ${HEADERS}
    --
  DEPENDS ${HEADER_SRCS}
  VERBATIM)

add_library(header_includes-wrapper SHARED wrapper.c)
target_link_libraries(header_includes-wrapper PRIVATE
  header_includes-original)
target_link_options(header_includes-wrapper PRIVATE
  -Wl,--version-script,${CMAKE_CURRENT_BINARY_DIR}/wrapper.c.syms)
target_include_directories(header_includes-wrapper BEFORE PRIVATE ${PROJECT_SOURCE_DIR}/../include)

# Copy the C file to the build directory so it can't pick up the original header
# from its source directory
configure_file(main.c main.c COPYONLY)
add_executable(header_includes-main ${CMAKE_CURRENT_BINARY_DIR}/main.c)
target_link_libraries(header_includes-main PRIVATE
  header_includes-wrapper)
target_include_directories(header_includes-main BEFORE PRIVATE ${PROJECT_SOURCE_DIR}/../include)

add_dependencies(check-ia2 header_includes-main)
