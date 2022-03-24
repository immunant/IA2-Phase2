cmake_minimum_required(VERSION 3.12)
project(libia2)

# TODO: find_package(cargo REQUIRED)


if(DEFINED ENV{CARGO_TARGET_DIR})
    set(CARGO_OUTPUT_DIR $ENV{CARGO_TARGET_DIR})
else()
    set(CARGO_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()
set(IA2_LIB_DIR ${CARGO_OUTPUT_DIR}/release)
set(IA2_LIB_DIR ${CARGO_OUTPUT_DIR}/release PARENT_SCOPE)
set(IA2_LIB ${IA2_LIB_DIR}/libia2.so)
set(IA2_LIB ${IA2_LIB_DIR}/libia2.so PARENT_SCOPE)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/padding.ld ${CMAKE_CURRENT_BINARY_DIR}/)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/shared_lib.ld ${CMAKE_CURRENT_BINARY_DIR}/)


set(LIBIA2_SRC src/lib.rs)

add_custom_target(libia2
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND cargo +nightly build
        "--release"
        ${LIBIA2_FEATURES}
        "--target-dir" ${CARGO_OUTPUT_DIR}
    COMMAND mv pkey_init.h ${IA2_INCLUDE_DIR}/
    BYPRODUCTS ${IA2_LIB}
    DEPENDS ${LIBIA2_SRC}
)
