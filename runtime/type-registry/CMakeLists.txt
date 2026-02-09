cmake_minimum_required(VERSION 4.0)
project(type-registry)

if(LIBIA2_AARCH64)
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
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}
    cargo build
      --profile ${CARGO_PROFILE}
      --target ${CARGO_TARGET}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml ${CMAKE_CURRENT_SOURCE_DIR}/src/lib.rs
)

add_library(type-registry STATIC IMPORTED GLOBAL)
set_property(TARGET type-registry PROPERTY IMPORTED_LOCATION ${LIB_PATH})
add_custom_target(type-registry-tgt DEPENDS ${LIB_PATH})
add_dependencies(type-registry type-registry-tgt)

if (NOT CMAKE_CROSSCOMPILING_EMULATOR)
  # It's more complex to test with qemu, so only test on native builds.
  # Should be sufficient as this lib is basically platform independent.
  add_test(
    NAME type-registry-cargo-test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E env
      CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}
      cargo test
        --profile ${CARGO_PROFILE}
        --target ${CARGO_TARGET}
  )
  set_tests_properties(type-registry-cargo-test PROPERTIES TIMEOUT 0)
endif()
