cmake_minimum_required(VERSION 3.8)
project(IA2HeaderRewriter)

find_package(Clang REQUIRED CONFIG)

add_definitions(${CLANG_DEFINITIONS} ${LLVM_DEFINITIONS})
include_directories(${CLANG_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS})

add_executable(ia2-header-rewriter
    HeaderRewriter.cpp
    )

target_link_libraries(ia2-header-rewriter PRIVATE
    clang-cpp
    LLVM
    )
