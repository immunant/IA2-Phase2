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

set(IA2_LINKER_SCRIPT_DIR ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
set(IA2_LINKER_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/padding.ld PARENT_SCOPE)
set(IA2_SHLIB_LINKER_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/shared_lib.ld PARENT_SCOPE)
set(IA2_DYNSYMS ${CMAKE_CURRENT_BINARY_DIR}/dynsym.syms PARENT_SCOPE)

add_custom_target(libia2_linker_script
    DEPENDS padding.ld shared_lib.ld dynsym.syms)

add_custom_command(
    COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/padding.ld .
    OUTPUT padding.ld
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/padding.ld ${CMAKE_CURRENT_SOURCE_DIR}/shared_lib.ld
)

add_custom_command(
    COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/shared_lib.ld .
    OUTPUT shared_lib.ld
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shared_lib.ld
)

add_custom_command(
    COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/dynsym.syms .
    OUTPUT dynsym.syms
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/dynsym.syms
)

set(LIBIA2_SRC src/lib.rs)

add_custom_target(libia2
    DEPENDS ${IA2_LIB})

add_custom_command(
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND cargo build
        "--release"
        ${LIBIA2_FEATURES}
        "--target-dir" ${CARGO_OUTPUT_DIR}
    COMMAND mv pkey_init.h ${IA2_INCLUDE_DIR}/
    OUTPUT ${IA2_LIB}
    DEPENDS ${LIBIA2_SRC}
)
