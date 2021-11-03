cmake_minimum_required(VERSION 3.12)
project(libia2)

# TODO: find_package(cargo REQUIRED)

set(LIBIA2_SRC
    src/lib.rs
    src/scrub_registers.s)

add_custom_target(libia2
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND cargo +nightly build
        "--release"
        "--features" insecure
        "--target-dir" ${CARGO_OUTPUT_DIR}
    BYPRODUCTS ${IA2_LIB}
    DEPENDS ${LIBIA2_SRC}
)
