cmake_minimum_required(VERSION 3.12)
project(libia2)

add_library(libia2 INTERFACE)

target_include_directories(libia2 INTERFACE include)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/padding.ld ${CMAKE_CURRENT_BINARY_DIR}/)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dynsym.syms ${CMAKE_CURRENT_BINARY_DIR}/)
