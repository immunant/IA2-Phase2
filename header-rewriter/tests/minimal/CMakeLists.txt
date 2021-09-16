add_library(original SHARED minimal.c)

add_custom_command(
  OUTPUT minimal.h wrapper.c
  COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/minimal.h ${CMAKE_CURRENT_BINARY_DIR}/minimal.h
  COMMAND ia2-header-rewriter
    ${CMAKE_CURRENT_BINARY_DIR}/wrapper.c
    ${CMAKE_CURRENT_BINARY_DIR}/minimal.h
    --
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/minimal.h
  VERBATIM)

add_library(wrapper SHARED wrapper.c)
target_link_libraries(wrapper PRIVATE
  original)
target_link_options(wrapper PRIVATE
  -Wl,--version-script,${CMAKE_CURRENT_BINARY_DIR}/wrapper.c.syms)
target_include_directories(wrapper BEFORE PRIVATE ${PROJECT_SOURCE_DIR}/../include)

# Copy the C file to the build directory so it can't pick up the original header
# from its source directory
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/main.c
  COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/main.c ${CMAKE_CURRENT_BINARY_DIR}/
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/main.c
  VERBATIM)
add_executable(main main.c)
target_link_libraries(main PRIVATE
  wrapper)
target_include_directories(main BEFORE PRIVATE ${PROJECT_SOURCE_DIR}/../include)

add_dependencies(check-ia2 main)
