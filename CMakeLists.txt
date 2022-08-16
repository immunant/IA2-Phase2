cmake_minimum_required(VERSION 3.12)
project(IA2Phase2)

find_package(PkgConfig REQUIRED)


set(IA2_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

include("cmake/define-ia2-wrapper.cmake")
include("cmake/define-test.cmake")
include("cmake/common-wrappers.cmake")

add_subdirectory(libia2)
add_subdirectory(header-rewriter)
