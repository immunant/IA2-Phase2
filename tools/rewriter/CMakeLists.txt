cmake_minimum_required(VERSION 3.12)
project(rewriter)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM: ${LLVM_DIR} (found version \"${LLVM_PACKAGE_VERSION}\")")
find_package(Clang REQUIRED CONFIG)
message(STATUS "Found Clang: ${Clang_DIR}")

add_definitions(${CLANG_DEFINITIONS} ${LLVM_DEFINITIONS})
include_directories(${CLANG_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS})

add_executable(ia2-rewriter
    DetermineAbi.cpp
    GenCallAsm.cpp
    SourceRewriter.cpp
    TypeOps.cpp
    )

set_target_properties(ia2-rewriter PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON)

target_compile_options(ia2-rewriter PRIVATE "-Wunused")
target_link_libraries(ia2-rewriter PRIVATE
    clang-cpp
    LLVM
    )