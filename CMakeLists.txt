cmake_minimum_required(VERSION 3.12)
project(IA2Phase2)

find_package(PkgConfig REQUIRED)


if(DEFINED ENV{CARGO_TARGET_DIR})
    set(CARGO_OUTPUT_DIR $ENV{CARGO_TARGET_DIR})
else()
    set(CARGO_OUTPUT_DIR ${PROJECT_SOURCE_DIR}/libia2/target/)
endif()
set(IA2_LIB ${CARGO_OUTPUT_DIR}/release/libia2.so)
set(IA2_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include/)


add_subdirectory(header-rewriter)
add_subdirectory(libia2)
