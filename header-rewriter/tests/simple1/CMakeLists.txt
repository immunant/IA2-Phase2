cmake_minimum_required(VERSION 3.20)
project(IA2TestSimple1)

set(IA2_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../include)
set(IA2_HEADER_REWRITER ${CMAKE_CURRENT_SOURCE_DIR}/../../build1/ia2-header-rewriter)

add_library(simple1 SHARED libsimple1/simple1.c)

add_custom_target(
  simple1-ia2
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1/simple1.h
    ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${IA2_HEADER_REWRITER} --output-header simple1_ia2.h simple1.h --
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1/simple1.h
  BYPRODUCTS simple1_ia2.h simple1.h
  VERBATIM
  )

add_executable(simple1-main main.c)
target_link_libraries(simple1-main PRIVATE simple1)
add_dependencies(simple1-main simple1-ia2)
target_compile_options(simple1-main PRIVATE -Werror=incompatible-pointer-types)
target_include_directories(simple1-main PRIVATE
  ${IA2_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1
  )
