cmake_minimum_required(VERSION 3.12)
project(IA2Runtime)

add_executable(read-pkru
  read_pkru_demo.c
  get_inferior_pkru.c
)

add_executable(track-memory-map
  memory_map.c
  track_memory_map_demo.c
  track_memory_map.c
  mmap_event.c
  get_inferior_pkru.c
)

add_executable(seccomp-filter
  seccomp_filter.c
)

add_executable(landlock
  landlock.c
  strv.c
  forbid_paths.c
)
