cmake_minimum_required(VERSION 3.12)
project(libia2)

add_library(libia2 SHARED ia2.c)

set_target_properties(libia2 PROPERTIES OUTPUT_NAME "ia2")

target_include_directories(libia2 PUBLIC include)
target_compile_definitions(libia2 PRIVATE _GNU_SOURCE)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/padding.ld ${CMAKE_CURRENT_BINARY_DIR}/)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/padding_lib.ld ${CMAKE_CURRENT_BINARY_DIR}/)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dynsym.syms ${CMAKE_CURRENT_BINARY_DIR}/)
