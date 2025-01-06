cmake_minimum_required(VERSION 3.12)
project(runtime)

add_subdirectory(libia2)
add_subdirectory(partition-alloc)
add_subdirectory(tracer)
add_subdirectory(type-registry)
