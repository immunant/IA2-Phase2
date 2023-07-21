cmake_minimum_required(VERSION 3.12)
project(libia2)

add_library(libia2 ia2.c threads.c main.c)
target_compile_options(libia2 PRIVATE "-fPIC")

if(LIBIA2_INSECURE)
target_compile_definitions(libia2 PRIVATE LIBIA2_INSECURE=1)
endif()

target_link_options(libia2
    INTERFACE
        "-pthread"
        "-Wl,--wrap=pthread_create"
        "-Wl,--wrap=main"
)

target_link_libraries(libia2 PRIVATE dl)

target_include_directories(libia2 PUBLIC include)
target_compile_definitions(libia2 PRIVATE _GNU_SOURCE)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/padding.ld ${CMAKE_CURRENT_BINARY_DIR}/)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dynsym.syms ${CMAKE_CURRENT_BINARY_DIR}/)
