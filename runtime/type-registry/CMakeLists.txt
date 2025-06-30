cmake_minimum_required(VERSION 4.0)
project(type-registry)

if (LIBIA2_AARCH64)
set(CARGO_TARGET "aarch64-unknown-linux-gnu")
else()
set(CARGO_TARGET "x86_64-unknown-linux-gnu")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
set(CARGO_PROFILE "dev")
set(CARGO_PROFILE_DIR "debug")
else()
set(CARGO_PROFILE "release")
set(CARGO_PROFILE_DIR "release")
endif()

set(LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/${CARGO_TARGET}/${CARGO_PROFILE_DIR}/libtype_registry.a)

add_custom_command(
  OUTPUT ${LIB_PATH}
  COMMAND ${CMAKE_COMMAND} -E env
    CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}
    cargo build
      --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml
      --profile ${CARGO_PROFILE}
      --target ${CARGO_TARGET}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml ${CMAKE_CURRENT_SOURCE_DIR}/src/lib.rs
)

add_library(type-registry STATIC IMPORTED GLOBAL)
set_property(TARGET type-registry PROPERTY IMPORTED_LOCATION ${LIB_PATH})
add_custom_target(type-registry-tgt DEPENDS ${LIB_PATH})
add_dependencies(type-registry type-registry-tgt)

add_test(
  NAME type-registry-test
  COMMAND ${CMAKE_COMMAND} -E env
    CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}
    cargo test
      --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml
      --profile ${CARGO_PROFILE}
      --target ${CARGO_TARGET}
)
