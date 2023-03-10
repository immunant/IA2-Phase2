cmake_minimum_required(VERSION 3.12)
project(libia2)

add_library(libia2 ia2.c threads.c main.c)
target_compile_options(libia2 PRIVATE "-fPIC")

if(LIBIA2_INSECURE)
    target_compile_definitions(libia2 PUBLIC LIBIA2_INSECURE=1)
endif()
if(LIBIA2_DEBUG)
    target_compile_definitions(libia2 PUBLIC LIBIA2_DEBUG=1)
endif()

target_link_options(libia2
    INTERFACE
        "-pthread"
        "-Wl,--wrap=pthread_create"
        "-Wl,--wrap=main"

        # Eagerly resolve GOT relocations
        "-Wl,-z,now"
        # So that we can mark all relocatable data as read-only after relocation
        "-Wl,-z,relro"
)

target_link_libraries(libia2 PRIVATE dl)

target_include_directories(libia2 PUBLIC include)
target_compile_definitions(libia2 PUBLIC _GNU_SOURCE)
set_target_properties(libia2 PROPERTIES POSITION_INDEPENDENT_CODE ON)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/padding.ld ${CMAKE_CURRENT_BINARY_DIR}/)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dynsym.syms ${CMAKE_CURRENT_BINARY_DIR}/)

add_subdirectory(test)
