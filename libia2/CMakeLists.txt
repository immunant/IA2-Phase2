cmake_minimum_required(VERSION 3.12)
project(libia2)

# TODO: find_package(cargo REQUIRED)

add_custom_target(libia2
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND cargo build "--release"
    BYPRODUCTS ${PROJECT_SOURCE_DIR}/target/release/libia2.so
)
