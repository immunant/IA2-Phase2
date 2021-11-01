set(ORIGINAL_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/)
add_library(original SHARED minimal.c)
target_include_directories(original BEFORE PRIVATE ${ORIGINAL_HEADER_DIR})
target_link_options(original PRIVATE "-Wl,-z,now")

# Build the wrapper lib
define_ia2_wrapper(
    WRAPPER wrapper
    WRAPPED_LIB original
    HEADERS minimal.h
)

# Copy the C file to the build directory so it can't pick up the original header
# from its source directory
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/main.c
  COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/main.c ${CMAKE_CURRENT_BINARY_DIR}/
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/main.c
  VERBATIM)
add_executable(main main.c)
target_link_libraries(main PRIVATE
  wrapper
  ${IA2_LIB})
target_include_directories(main BEFORE PRIVATE ${IA2_INCLUDE_DIR})

add_dependencies(check-ia2 main)
