cmake_minimum_required(VERSION 4.0)
project(IA2Phase2)

# Feature flags
option(IA2_DEBUG "Enable debug telemetry and runtime checks" OFF)

# IA2_LIBC_COMPARTMENT is only available on x86-64 (not aarch64).
# When enabled, code can assume x86-64 without checking LIBIA2_AARCH64.
# Enabling this option automatically rebuilds glibc with MPK support and
# adds --libc-compartment to all rewriter invocations.
include(CMakeDependentOption)
cmake_dependent_option(IA2_LIBC_COMPARTMENT
    "Enable libc/ld.so compartmentalization and exit callgate support" OFF
    "NOT LIBIA2_AARCH64" OFF)

if(IA2_DEBUG)
    message(STATUS "IA2: debug mode enabled")
endif()

if(IA2_LIBC_COMPARTMENT)
    message(STATUS "IA2: libc compartmentalization enabled (implies custom glibc build)")
    add_compile_definitions(IA2_LIBC_COMPARTMENT=1)
    # Libc compartmentalization requires the custom glibc with MPK-protected loader heap
    set(LIBIA2_REBUILD_GLIBC ON)
endif()

include(ExternalProject)
include(CTest)
include(cmake/ia2.cmake)
include(cmake/qemu-command.cmake)

find_package(PkgConfig REQUIRED)

set(EXTERNAL_DIR ${PROJECT_SOURCE_DIR}/external)

enable_testing()

# Set up 'check' target
if (LIBIA2_AARCH64)
    # On aarch64, avoid swallowing QEMU MTE check failures
    set(CMAKE_CTEST_COMMAND ${CMAKE_CTEST_COMMAND} --verbose)
endif()
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --timeout 60 --output-on-failure)
set_target_properties(check PROPERTIES FOLDER ".")

# runtime needs to be first so it defines libia2_BINARY_DIR
add_subdirectory(runtime)

add_subdirectory(benchmarks)

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
