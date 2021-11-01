cmake_minimum_required(VERSION 3.12)
project(IA2Phase2)

find_package(PkgConfig REQUIRED)

set(IA2_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include/)
set(IA2_LIB ${PROJECT_SOURCE_DIR}/libia2/target/release/libia2.so)

add_subdirectory(header-rewriter)
add_subdirectory(libia2)
