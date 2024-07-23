cmake_minimum_required(VERSION 3.13)
project(IA2Phase2)
include(ExternalProject)

find_package(PkgConfig REQUIRED)

include(cmake/ia2.cmake)

set(EXTERNAL_DIR ${PROJECT_SOURCE_DIR}/external)

# runtime needs to be first so it defines libia2_BINARY_DIR
add_subdirectory(runtime)

if (NOT LIBIA2_AARCH64)
    add_subdirectory(examples)
endif()
add_subdirectory(misc)

add_subdirectory(tests)
ExternalProject_Add(tools
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/tools
    BINARY_DIR ${CMAKE_BINARY_DIR}/tools
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DClang_DIR=${Clang_DIR}
        -DLLVM_DIR=${LLVM_DIR}
    INSTALL_COMMAND "")
