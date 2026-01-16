cmake_minimum_required(VERSION 4.0)
project(libia2)

# Detect system library paths for custom loader rpath (needed on x86_64 with REBUILD_GLIBC)
# Use CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES which contains actual runtime library paths,
# not linker script directories that find_library(stdc++) would return.
if(LIBIA2_REBUILD_GLIBC AND NOT LIBIA2_AARCH64)
    set(CUSTOM_LOADER_RPATH_FLAGS "")
    foreach(dir IN LISTS CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES)
        list(APPEND CUSTOM_LOADER_RPATH_FLAGS "-Wl,-rpath,${dir}")
    endforeach()
endif()

set(LIBIA2_SOURCES
    ia2.c
    init.c
    threads.c
    main.c
    exit.c
    memory_maps.c
    thread_name.c)

if(IA2_LIBC_COMPARTMENT)
    list(APPEND LIBIA2_SOURCES
        exit_callgates.c
    )
endif()

add_library(libia2 ${LIBIA2_SOURCES})
target_compile_options(libia2 PRIVATE "-fPIC")

if (LIBIA2_AARCH64)
    target_compile_options(libia2 PUBLIC "-march=armv8.5-a+memtag" "-ffixed-x18")
else()
endif()

if(IA2_DEBUG)
    target_compile_definitions(libia2 PUBLIC IA2_DEBUG=1)
endif()

if(IA2_VERBOSE)
    target_compile_definitions(libia2 PRIVATE IA2_VERBOSE=1)
endif()

if(IA2_DEBUG_MEMORY)
    target_compile_definitions(libia2 PRIVATE IA2_DEBUG_MEMORY=1)
endif()

target_link_options(libia2
    INTERFACE
        "-pthread"
        "-Wl,--wrap=pthread_create"
        "-Wl,--wrap=main"
        "-Wl,--wrap=free" # Because we use `__real_free` in `memory_maps.c`.
        $<$<BOOL:${IA2_LIBC_COMPARTMENT}>:-Wl,--wrap=__cxa_finalize>

        # Eagerly resolve GOT relocations
        "-Wl,-z,now"
        # So that we can mark all relocatable data as read-only after relocation
        "-Wl,-z,relro"

        # When rebuilding glibc on x86_64, embed the custom loader path in PT_INTERP
        # and add the build directory to rpath so the loader can find libc.so.6.
        $<$<AND:$<BOOL:${LIBIA2_REBUILD_GLIBC}>,$<NOT:$<BOOL:${LIBIA2_AARCH64}>>>:-Wl,--dynamic-linker=${CMAKE_CURRENT_BINARY_DIR}/ld-linux-x86-64.so.2>
        $<$<AND:$<BOOL:${LIBIA2_REBUILD_GLIBC}>,$<NOT:$<BOOL:${LIBIA2_AARCH64}>>>:-Wl,-rpath,${CMAKE_CURRENT_BINARY_DIR}>
        # Include system library paths so custom loader can find libraries like libstdc++
        ${CUSTOM_LOADER_RPATH_FLAGS}
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
set(CARGO_PROFILE "dev")
set(CARGO_PROFILE_DIR "debug")
else()
set(CARGO_PROFILE "release")
set(CARGO_PROFILE_DIR "release")
endif()

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

if (LIBIA2_AARCH64)
    set(SYSROOT_BIN_DIR ${CMAKE_BINARY_DIR}/external/glibc/sysroot)
    set(LIBC_PATH ${SYSROOT_BIN_DIR}/lib/libc.so.6)
    set(LD_LINUX_PATH ${SYSROOT_BIN_DIR}/lib/ld-linux-aarch64.so.1)
    if (LIBIA2_REBUILD_GLIBC)
        ExternalProject_Add(glibc
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/glibc
            BINARY_DIR ${CMAKE_BINARY_DIR}/external/glibc
            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
                # ffixed-x18 has to be set here instead of CFLAGS for it to apply to all files
                "CC=${CMAKE_C_COMPILER} -target aarch64-linux-gnu -ffixed-x18 -march=armv8.5-a+memtag --rtlib=compiler-rt -O3"
                # this is needed for some test files built by the build system
                "CXX=${CMAKE_CXX_COMPILER} -target aarch64-linux-gnu -ffixed-x18 -march=armv8.5-a+memtag --rtlib=compiler-rt -O3"
                ${CMAKE_SOURCE_DIR}/external/glibc/configure
                "--build=aarch64-linux-gnu"
                "--target=aarch64-linux-gnu"
                "--prefix=${CMAKE_BINARY_DIR}/external/glibc/sysroot"
                # build with aarch64 binutils installed on the system
                "--with-headers=/usr/aarch64-linux-gnu/include"
                "--with-binutils=/usr/aarch64-linux-gnu/bin"
                # This is only for the heap tunable
                "--enable-memory-tagging"
            BUILD_COMMAND make -j8
            # places the built libc.so and ld.so at the locations expected by QEMU's LD_LIBPRARY_PATH
            INSTALL_COMMAND make install
            USES_TERMINAL_CONFIGURE TRUE
            USES_TERMINAL_BUILD TRUE
            USES_TERMINAL_INSTALL TRUE
            BUILD_BYPRODUCTS ${LIBC_PATH} ${LD_LINUX_PATH}
        )
    else()
        set(SYSROOT_SRC_DIR ${CMAKE_SOURCE_DIR}/external/glibc_sysroot)
        file(GLOB SYSROOT_HEADERS_SRC
            ${SYSROOT_SRC_DIR}/include/*.h
            ${SYSROOT_SRC_DIR}/include/*/*.h
            ${SYSROOT_SRC_DIR}/include/*/*/*.h)
        string(REGEX REPLACE ${SYSROOT_SRC_DIR} ${SYSROOT_BIN_DIR} ${SYSROOT_HEADERS_SRC} SYSROOT_HEADERS)
        add_custom_command(
            OUTPUT
                ${SYSROOT_BIN_DIR}/lib/libc.so.6
                ${SYSROOT_BIN_DIR}/lib/ld-linux-aarch64.so.1
                ${SYSROOT_HEADERS}
            COMMAND ${CMAKE_COMMAND} -E copy ${SYSROOT_SRC_DIR}/lib/libc.so.6 ${SYSROOT_BIN_DIR}/lib/
            COMMAND ${CMAKE_COMMAND} -E copy ${SYSROOT_SRC_DIR}/lib/ld-linux-aarch64.so.1 ${SYSROOT_BIN_DIR}/lib/
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${SYSROOT_SRC_DIR}/include ${SYSROOT_BIN_DIR}/usr/include
            COMMENT "Copying glibc prebuilts to sysroot"
        )
        add_custom_target(glibc
            DEPENDS ${SYSROOT_BIN_DIR}/lib/libc.so.6 ${SYSROOT_BIN_DIR}/lib/ld-linux-aarch64.so.1 ${SYSROOT_HEADERS}
        )
    endif()
else()
    # x86_64 path
    if(LIBIA2_REBUILD_GLIBC)
        # Build glibc from source for x86_64 (native build)
        include(ProcessorCount)
        ProcessorCount(NPROC)
        if(NPROC EQUAL 0)
            set(NPROC 4)
        endif()

        set(GLIBC_SYSROOT ${CMAKE_BINARY_DIR}/external/glibc/sysroot)
        set(LIBC_PATH ${GLIBC_SYSROOT}/lib/libc.so.6)
        set(LD_LINUX_X86_PATH ${GLIBC_SYSROOT}/lib/ld-linux-x86-64.so.2)

        ExternalProject_Add(glibc
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/glibc
            BINARY_DIR ${CMAKE_BINARY_DIR}/external/glibc
            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
                "CC=${CMAKE_C_COMPILER}"
                "CXX=${CMAKE_CXX_COMPILER}"
                "CFLAGS=-O2 -g"
                ${CMAKE_SOURCE_DIR}/external/glibc/configure
                "--prefix=${GLIBC_SYSROOT}"
                "--disable-werror"
                "--disable-multilib"
                "--disable-nscd"
                "--disable-timezone-tools"
            BUILD_COMMAND make -j${NPROC}
            INSTALL_COMMAND make install
            USES_TERMINAL_CONFIGURE TRUE
            USES_TERMINAL_BUILD TRUE
            USES_TERMINAL_INSTALL TRUE
            BUILD_BYPRODUCTS ${LIBC_PATH} ${LD_LINUX_X86_PATH}
        )
    else()
        # Use system libc (existing behavior)
        # Find libc. We cannot simply do `find_library(LIBC c REQUIRED)` because
        # `libc.so` itself is a linker script with glibc, while we need the path of
        # the actual ELF shared object itself so we can patch its program headers.
        set(LIBC_PATHS /lib /lib64 /lib/x86_64-linux-gnu)

        foreach(CANDIDATE IN LISTS LIBC_PATHS)
            if(EXISTS ${CANDIDATE}/libc.so.6)
                set(LIBC_PATH ${CANDIDATE}/libc.so.6)
            endif()
        endforeach()
    endif()
endif()


if(NOT DEFINED LIBC_PATH)
    message(FATAL_ERROR "Could not find libc.so.6 in: ${LIBC_PATHS}\n")
endif()

# Generate libc with padded TLS segment
# When rebuilding glibc, we must wait for the glibc build to complete
if(LIBIA2_REBUILD_GLIBC)
    set(LIBC_PADDING_DEPENDS ${PAD_TLS_SRCS} tools pad-tls glibc ${LIBC_PATH})
else()
    set(LIBC_PADDING_DEPENDS ${PAD_TLS_SRCS} tools pad-tls ${LIBC_PATH})
endif()
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libc.so.6
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBC_PATH} ${CMAKE_CURRENT_BINARY_DIR}/libc.so.6
    COMMAND ${CMAKE_BINARY_DIR}/tools/pad-tls/pad-tls --allow-no-tls ${CMAKE_CURRENT_BINARY_DIR}/libc.so.6
    DEPENDS ${LIBC_PADDING_DEPENDS}
    COMMENT "Padding TLS segment of libc"
)

# Explicitly depend on ld.so because we must use one built with our toolchain
if(LIBIA2_AARCH64)
    if(NOT DEFINED LD_LINUX_PATH)
        message(FATAL_ERROR "Could not find ld-linux-aarch64.so.1 in: ${LIBC_PATHS}\n")
    endif()

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ld-linux-aarch64.so.1
        COMMAND ${CMAKE_COMMAND} -E copy ${LD_LINUX_PATH} ${CMAKE_CURRENT_BINARY_DIR}/ld-linux-aarch64.so.1
        DEPENDS ${LD_LINUX_PATH}
        COMMENT "Copying ld-linux-aarch64.so.1"
    )
    add_library(libc SHARED IMPORTED)
    set_property(TARGET libc PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/libc.so.6")
    target_link_libraries(libia2 PUBLIC libc)
    add_library(ld-linux SHARED IMPORTED)
    set_property(TARGET ld-linux PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/ld-linux-aarch64.so.1")
    target_link_libraries(libia2 PUBLIC ld-linux)
    add_custom_target(libc-ia2 DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libc.so.6" "${CMAKE_CURRENT_BINARY_DIR}/ld-linux-aarch64.so.1")
elseif(LIBIA2_REBUILD_GLIBC)
    # x86_64 with custom glibc
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ld-linux-x86-64.so.2
        COMMAND ${CMAKE_COMMAND} -E copy ${LD_LINUX_X86_PATH} ${CMAKE_CURRENT_BINARY_DIR}/ld-linux-x86-64.so.2
        DEPENDS glibc ${LD_LINUX_X86_PATH}
        COMMENT "Copying ld-linux-x86-64.so.2"
    )
    add_library(libc SHARED IMPORTED)
    set_property(TARGET libc PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/libc.so.6")
    target_link_libraries(libia2 PUBLIC libc)
    add_library(ld-linux SHARED IMPORTED)
    set_property(TARGET ld-linux PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/ld-linux-x86-64.so.2")
    target_link_libraries(libia2 PUBLIC ld-linux)
    add_custom_target(libc-ia2 DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libc.so.6" "${CMAKE_CURRENT_BINARY_DIR}/ld-linux-x86-64.so.2")
else()
    # x86_64 with system libc (no custom loader)
    add_custom_target(libc-ia2 DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libc.so.6")
endif()


if(LIBIA2_AARCH64 OR LIBIA2_REBUILD_GLIBC)
    add_dependencies(libc-ia2 glibc)
endif()

add_dependencies(libia2 libc-ia2)
target_link_libraries(libia2 PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/libc.so.6")
if (LIBIA2_AARCH64)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include
        OUTPUT_VARIABLE COMPILER_HEADERS_INCLUDE
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include-fixed
        OUTPUT_VARIABLE COMPILER_HEADERS_INCLUDE_FIXED
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    target_compile_options(libia2 PUBLIC
        -isystem${CMAKE_BINARY_DIR}/external/glibc/sysroot
        -isystem${COMPILER_HEADERS_INCLUDE}
        -isystem${COMPILER_HEADERS_INCLUDE_FIXED})
endif()
