cmake_minimum_required(VERSION 3.12)
project(IA2HeaderRewriter)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# We use the lit CMake functions from LLVM
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_definitions(${CLANG_DEFINITIONS} ${LLVM_DEFINITIONS})
include_directories(${CLANG_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS})

add_executable(ia2-header-rewriter
    HeaderRewriter.cpp
    DetermineAbi.cpp
    GenCallAsm.cpp
    )

set_property(TARGET ia2-header-rewriter PROPERTY
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON)

target_link_libraries(ia2-header-rewriter PRIVATE
    clang-cpp
    LLVM
    )

set(IA2_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(IA2_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(tests)
