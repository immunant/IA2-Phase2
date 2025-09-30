cmake_minimum_required(VERSION 4.0)
project(rewriter)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM: ${LLVM_DIR} (found version \"${LLVM_PACKAGE_VERSION}\")")
find_package(Clang REQUIRED CONFIG)
message(STATUS "Found Clang: ${Clang_DIR}")

# We use the lit CMake functions from LLVM
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_executable(ia2-rewriter
    Context.cpp
    DetermineAbi.cpp
    GenCallAsm.cpp
    SourceRewriter.cpp
    TypeOps.cpp
)

set_target_properties(ia2-rewriter PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON)

target_compile_definitions(ia2-rewriter PRIVATE
    ${CLANG_DEFINITIONS}
    ${LLVM_DEFINITIONS})

target_include_directories(ia2-rewriter PRIVATE
    ${CLANG_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS})

target_compile_options(ia2-rewriter PRIVATE "-Wunused")
target_link_libraries(ia2-rewriter PRIVATE
    clang-cpp
    LLVM
    )

if(IA2_TRACE_EXIT)
    target_compile_definitions(ia2-rewriter PRIVATE IA2_TRACE_EXIT=1)
endif()
