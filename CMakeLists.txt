cmake_minimum_required(VERSION 3.12)
project(IA2Phase2)

find_package(PkgConfig REQUIRED)

set(EXTERNAL_DIR ${PROJECT_SOURCE_DIR}/external)

# libia2 needs to be first so it defines ${libia2_BINARY_DIR}
add_subdirectory(libia2)

add_subdirectory(examples)
add_subdirectory(rewriter)
add_subdirectory(partition-alloc)
add_subdirectory(pad-tls)
