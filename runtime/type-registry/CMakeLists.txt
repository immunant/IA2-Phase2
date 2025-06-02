cmake_minimum_required(VERSION 4.0)
project(type-registry)

if (LIBIA2_AARCH64)
set(CARGO_TARGET_FLAG "--target=aarch64-unknown-linux-gnu" "--config" "target.aarch64-unknown-linux-gnu.linker=\\\"aarch64-linux-gnu-gcc\\\"")
set(CARGO_ARCH_SUFFIX "aarch64-unknown-linux-gnu")
else()
set(CARGO_TARGET_FLAG "")
set(CARGO_ARCH_SUFFIX "")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
set(CARGO_PROFILE "dev")
set(CARGO_PROFILE_DIR "debug")
else()
set(CARGO_PROFILE "release")
set(CARGO_PROFILE_DIR "release")
endif()

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${CARGO_ARCH_SUFFIX}/${CARGO_PROFILE_DIR}/libtype_registry.a
  COMMAND CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR} cargo build ${CARGO_TARGET_FLAG} --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml --profile ${CARGO_PROFILE}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml ${CMAKE_CURRENT_SOURCE_DIR}/src/lib.rs
)

add_library(type-registry STATIC IMPORTED GLOBAL)
set_property(TARGET type-registry PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/${CARGO_ARCH_SUFFIX}/${CARGO_PROFILE_DIR}/libtype_registry.a)
add_custom_target(type-registry-tgt DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${CARGO_ARCH_SUFFIX}/${CARGO_PROFILE_DIR}/libtype_registry.a)
add_dependencies(type-registry type-registry-tgt)
