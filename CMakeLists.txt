cmake_minimum_required(VERSION 3.13)
project(IA2Phase2)

find_package(PkgConfig REQUIRED)

include(cmake/ia2.cmake)

set(EXTERNAL_DIR ${PROJECT_SOURCE_DIR}/external)

# runtime needs to be first so it defines libia2_BINARY_DIR
add_subdirectory(runtime)

add_subdirectory(examples)
add_subdirectory(tests)
add_subdirectory(tools)
