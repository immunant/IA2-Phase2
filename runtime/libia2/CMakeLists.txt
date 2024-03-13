cmake_minimum_required(VERSION 3.12)
project(libia2)

add_library(libia2 ia2.c init.c threads.c main.c exit.c)
target_compile_options(libia2 PRIVATE "-fPIC")

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
target_compile_definitions(libia2 PRIVATE IA2_ENABLE=1)
set_target_properties(libia2 PROPERTIES POSITION_INDEPENDENT_CODE ON)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/padding.ld ${CMAKE_CURRENT_BINARY_DIR}/)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dynsym.syms ${CMAKE_CURRENT_BINARY_DIR}/)

target_link_options(libia2 INTERFACE
    "-Wl,-T${CMAKE_CURRENT_BINARY_DIR}/padding.ld"
    "-Wl,--dynamic-list=${CMAKE_CURRENT_BINARY_DIR}/dynsym.syms"
)
set_property(TARGET libia2 PROPERTY INTERFACE_LINK_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/padding.ld ${CMAKE_CURRENT_BINARY_DIR}/dynsym.syms)

# Find libc. We cannot simply do `find_library(LIBC c REQUIRED)` because
# `libc.so` itself is a linker script with glibc, while we need the path of
# the actual ELF shared object itself so we can patch its program headers.
set(LIBC_PATHS /lib /lib64 /lib/x86_64-linux-gnu)
foreach(CANDIDATE in ${LIBC_PATHS})
    if (EXISTS ${CANDIDATE}/libc.so.6)
        set(LIBC_PATH ${CANDIDATE}/libc.so.6)
    endif()
endforeach()
if(NOT DEFINED LIBC_PATH)
    message(FATAL_ERROR "Could not find libc.so.6 in: ${LIBC_PATHS}\n")
endif()

# Generate libc with padded TLS segment
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libc.so.6
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBC_PATH} ${CMAKE_CURRENT_BINARY_DIR}/libc.so.6
    COMMAND pad-tls --allow-no-tls ${CMAKE_CURRENT_BINARY_DIR}/libc.so.6
    DEPENDS pad-tls
    COMMENT "Padding TLS segment of libc"
)

add_custom_target(libc-ia2 DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libc.so.6")

add_dependencies(libia2 libc-ia2)
target_link_libraries(libia2 PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/libc.so.6")
