cmake_minimum_required(VERSION 3.8)
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
    )

target_link_libraries(ia2-header-rewriter PRIVATE
    clang-cpp
    LLVM
    )

set(IA2_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(IA2_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(tests)
get_property(LLVM_LIT_TESTSUITES GLOBAL PROPERTY LLVM_LIT_TESTSUITES)
get_property(LLVM_LIT_PARAMS GLOBAL PROPERTY LLVM_LIT_PARAMS)
get_property(LLVM_LIT_DEPENDS GLOBAL PROPERTY LLVM_LIT_DEPENDS)
get_property(LLVM_LIT_EXTRA_ARGS GLOBAL PROPERTY LLVM_LIT_EXTRA_ARGS)
get_property(LLVM_ADDITIONAL_TEST_TARGETS
    GLOBAL PROPERTY LLVM_ADDITIONAL_TEST_TARGETS)
get_property(LLVM_ADDITIONAL_TEST_DEPENDS
    GLOBAL PROPERTY LLVM_ADDITIONAL_TEST_DEPENDS)
add_lit_target(check-all
    "Running all regression tests"
    ${LLVM_LIT_TESTSUITES}
    PARAMS ${LLVM_LIT_PARAMS}
    DEPENDS ${LLVM_LIT_DEPENDS} ${LLVM_ADDITIONAL_TEST_TARGETS}
    ARGS ${LLVM_LIT_EXTRA_ARGS}
    )
