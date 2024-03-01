cmake_minimum_required(VERSION 3.12)
project(tracer)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/release/libmemory_map.so
  COMMAND CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR} cargo build --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/memory-map/Cargo.toml --release
  DEPENDS memory-map/Cargo.toml memory-map/src/lib.rs
)

add_library(memory-map STATIC IMPORTED)
set_property(TARGET memory-map PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/release/libmemory_map.so)
add_custom_target(memory-map-tgt DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/release/libmemory_map.so)
add_dependencies(memory-map memory-map-tgt)

add_executable(read-pkru
  read_pkru_demo.c
  get_inferior_pkru.c
)

add_executable(track-memory-map
  track_memory_map_demo.c
  track_memory_map.c
  mmap_event.c
  get_inferior_pkru.c
)
target_link_libraries(track-memory-map PRIVATE memory-map)

add_executable(seccomp-filter
  seccomp_filter_demo.c
  seccomp_filter.c
)

add_executable(landlock
  landlock_demo.c
  landlock.c
  strv.c
  forbid_paths.c
)

add_executable(ia2-sandbox
  start.c
  seccomp_filter.c
  track_memory_map.c
  mmap_event.c
  get_inferior_pkru.c
  landlock.c
  strv.c
  forbid_paths.c
)
target_link_libraries(ia2-sandbox PRIVATE memory-map)
