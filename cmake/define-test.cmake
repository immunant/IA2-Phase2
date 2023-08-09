# Get system header directories for header rewriter as it needs them explicitly passed
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include
  OUTPUT_VARIABLE C_SYSTEM_INCLUDE
  OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include-fixed
  OUTPUT_VARIABLE C_SYSTEM_INCLUDE_FIXED
  OUTPUT_STRIP_TRAILING_WHITESPACE)

set(PADDING_LINKER_SCRIPT ${libia2_BINARY_DIR}/padding.ld)

# Creates wrapped and unmodified shared library targets for a test
#
# NEEDS_LD_WRAP - If present pass -Wl,@$LD_ARGS_FILE to the wrapped
#       library. The name of the file is generated from the target name and pkey.
# PKEY (optional) - The pkey for the wrapped library (defaults to 0).
# LIBNAME (optional) - The shared library name (defaults to ${TEST_NAME}_lib)
# SRCS (required) - The set of source files
# INCLUDE_DIR (optional) - include directory used to build the shared lib. If
#       specified, this must be relative to the source directory for the test.
#       Defaults to include/. If headers are also used for main executable, this
#       must not be specified.
function(define_shared_lib)
    set(options NEEDS_LD_WRAP)
    set(oneValueArgs LIBNAME PKEY)
    set(multiValueArgs SRCS INCLUDE_DIR)
    cmake_parse_arguments(SHARED_LIB "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

    # Set library and wrapped library target names
    get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    if(DEFINED SHARED_LIB_LIBNAME)
        set(LIBNAME ${SHARED_LIB_LIBNAME})
    else()
        set(LIBNAME ${TEST_NAME}_lib)
    endif()
    set(WRAPPED_LIBNAME ${LIBNAME}_wrapped)

    if (NOT DEFINED SHARED_LIB_PKEY)
        set(SHARED_LIB_PKEY "0")
    endif()

    # INCLUDE_DIR is relative to the test target directory
    if(DEFINED SHARED_LIB_INCLUDE_DIR)
        set(RELATIVE_INCLUDE_DIR ${SHARED_LIB_INCLUDE_DIR})
    else()
        set(RELATIVE_INCLUDE_DIR include)
    endif()
    set(ORIGINAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${RELATIVE_INCLUDE_DIR})
    set(REWRITTEN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/${RELATIVE_INCLUDE_DIR})
    
    set(ORIGINAL_SRCS ${SHARED_LIB_SRCS})
    list(TRANSFORM ORIGINAL_SRCS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
    set(COPIED_SRCS ${SHARED_LIB_SRCS})
    list(TRANSFORM COPIED_SRCS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)

    if(DEFINED SHARED_LIB_INCLUDE_DIR)
        file(GLOB ORIGINAL_LIB_HEADERS "${ORIGINAL_INCLUDE_DIR}/*.h")
        list(TRANSFORM ORIGINAL_LIB_HEADERS REPLACE
            ${ORIGINAL_INCLUDE_DIR} ${REWRITTEN_INCLUDE_DIR}
            OUTPUT_VARIABLE COPIED_LIB_HEADERS)
        add_custom_command(
            OUTPUT ${COPIED_SRCS} ${COPIED_LIB_HEADERS}
            COMMAND cp ${ORIGINAL_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/
            COMMAND mkdir -p ${REWRITTEN_INCLUDE_DIR}
            COMMAND cp ${ORIGINAL_LIB_HEADERS} ${REWRITTEN_INCLUDE_DIR}/
            DEPENDS ${ORIGINAL_SRCS} ${ORIGINAL_LIB_HEADERS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_custom_target(copy_files_${LIBNAME} DEPENDS ${COPIED_SRCS} ${COPIED_LIB_HEADERS})
    else()
        add_custom_command(
            OUTPUT ${COPIED_SRCS}
            COMMAND cp ${ORIGINAL_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/
            DEPENDS ${ORIGINAL_SRCS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_custom_target(copy_files_${LIBNAME} DEPENDS ${COPIED_SRCS})
    endif()

    # Define two targets
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    add_library(${LIBNAME} SHARED ${COPIED_SRCS})
    set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
    add_library(${WRAPPED_LIBNAME} SHARED ${COPIED_SRCS})
    add_dependencies(${WRAPPED_LIBNAME} ${TEST_NAME}_call_gate_generation)
    set_target_properties(${LIBNAME} PROPERTIES EXCLUDE_FROM_ALL 1)

    target_compile_definitions(${LIBNAME} PRIVATE PRE_REWRITER=1)
    # Define options common to both targets
    foreach(target ${LIBNAME} ${WRAPPED_LIBNAME})
        set_target_properties(${target} PROPERTIES PKEY ${SHARED_LIB_PKEY})

        if (DEFINED SHARED_LIB_PKEY)
            target_compile_definitions(${target} PRIVATE PKEY=${SHARED_LIB_PKEY})
        endif()
        target_compile_options(${target} PRIVATE
            "-Werror=incompatible-pointer-types"
            "-fsanitize=undefined"
            "-fPIC"
            "-g")

        if (${SHARED_LIB_PKEY} GREATER 0)
            target_include_directories(${target} BEFORE PRIVATE ${REWRITTEN_INCLUDE_DIR})
        else()
            target_include_directories(${target} BEFORE PRIVATE ${ORIGINAL_INCLUDE_DIR})
        endif()
        target_compile_options(${target} PRIVATE
            "-isystem${C_SYSTEM_INCLUDE}"
            "-isystem${C_SYSTEM_INCLUDE_FIXED}")
        target_link_options(${target} PRIVATE
            # UBSAN requires passing this as both a compiler and linker flag
            "-fsanitize=undefined")
        target_link_libraries(${target} PRIVATE
            partition-alloc
            libia2)
    endforeach()

    target_link_libraries(${WRAPPED_LIBNAME} PRIVATE ${TEST_NAME}_call_gates)
    if (SHARED_LIB_NEEDS_LD_WRAP)
        set_target_properties(${WRAPPED_LIBNAME} PROPERTIES NEEDS_LD_WRAP YES)
        target_link_options(${WRAPPED_LIBNAME} PRIVATE
            "-Wl,@${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates_${SHARED_LIB_PKEY}.ld")
    else()
        set_target_properties(${WRAPPED_LIBNAME} PROPERTIES NEEDS_LD_WRAP NO)
    endif()
    if (${SHARED_LIB_PKEY} GREATER 0)
        target_link_libraries(${WRAPPED_LIBNAME} PRIVATE
            "-Wl,-T${PADDING_LINKER_SCRIPT}")
        set_target_properties(${WRAPPED_LIBNAME} PROPERTIES LINK_DEPENDS ${PADDING_LINKER_SCRIPT})
        target_compile_options(${WRAPPED_LIBNAME} PRIVATE
            "-include${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates.h")
        add_tls_padded_library(
            LIB "${WRAPPED_LIBNAME}"
            OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/padded")
    endif()
endfunction()


# Creates wrapped and unmodified executable targets for a test
#

# NEEDS_LD_WRAP - If present pass -Wl,@$LD_ARGS_FILE to the wrapped
#       library. The name of the file is generated from the target name and pkey.
# PKEY (optional) - The pkey for the wrapped library (defaults to 1).
# LIBS (optional) - libraries to link the unmodified build against (defaults to
#       ${TEST_NAME}_lib which is the default set by define_shared_lib).
# SRCS (required) - The set of source files
# INCLUDE_DIR (optional) - include directory used to build the executable. If
#       specified, this must be relative to the source directory for the test.
#       Defaults to include/. If headers are also used for shared libraries, this
#       must not be specified.
function(define_test)
    # Parse options
    set(options NEEDS_LD_WRAP)
    set(oneValueArgs PKEY)
    set(multiValueArgs LIBS SRCS INCLUDE_DIR)
    cmake_parse_arguments(DEFINE_TEST "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

    # Set executable and wrapper target names
    get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    set(MAIN ${TEST_NAME}_main)
    set(WRAPPED_MAIN ${TEST_NAME}_main_wrapped)
    set(REWRAPPED_MAIN ${TEST_NAME}_main_rewrapped)

    if (NOT DEFINED DEFINE_TEST_PKEY)
        set(DEFINE_TEST_PKEY "1")
    endif()

    if (NOT DEFINED DEFINE_TEST_LIBS)
        set(DEFINE_TEST_LIBS ${TEST_NAME}_lib)
    endif()

    if(DEFINED DEFINE_TEST_INCLUDE_DIR)
        set(RELATIVE_INCLUDE_DIR ${DEFINE_TEST_INCLUDE_DIR})
    else()
        set(RELATIVE_INCLUDE_DIR include)
    endif()
    set(REWRAPPED_DIR "${CMAKE_CURRENT_BINARY_DIR}/rewrapped")
    set(ORIGINAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${RELATIVE_INCLUDE_DIR})
    set(REWRITTEN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/${RELATIVE_INCLUDE_DIR})
    set(REWRAPPED_INCLUDE_DIR ${REWRAPPED_DIR}/${RELATIVE_INCLUDE_DIR})

    set(ORIGINAL_SRCS ${DEFINE_TEST_SRCS})
    list(TRANSFORM ORIGINAL_SRCS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
    set(COPIED_SRCS ${DEFINE_TEST_SRCS})
    list(TRANSFORM COPIED_SRCS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)
    set(COPIED_SRCS_2 ${DEFINE_TEST_SRCS})
    list(TRANSFORM COPIED_SRCS_2 PREPEND ${REWRAPPED_DIR}/)

    file(GLOB ORIGINAL_HEADERS "${ORIGINAL_INCLUDE_DIR}/*.h")
    list(TRANSFORM ORIGINAL_HEADERS REPLACE
        ${ORIGINAL_INCLUDE_DIR} ${REWRITTEN_INCLUDE_DIR}
        OUTPUT_VARIABLE COPIED_HEADERS)
    add_custom_command(
        OUTPUT ${COPIED_SRCS} ${COPIED_HEADERS}
        COMMAND cp ${ORIGINAL_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/
        COMMAND mkdir -p ${REWRITTEN_INCLUDE_DIR}
        COMMAND cp ${ORIGINAL_HEADERS} ${REWRITTEN_INCLUDE_DIR}/
        DEPENDS ${ORIGINAL_SRCS} ${ORIGINAL_HEADERS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_custom_target(
        copy_files_${MAIN}
        DEPENDS ${COPIED_SRCS} ${COPIED_HEADERS}
    )
    list(TRANSFORM ORIGINAL_HEADERS REPLACE
        ${ORIGINAL_INCLUDE_DIR} ${REWRAPPED_INCLUDE_DIR}
        OUTPUT_VARIABLE COPIED_HEADERS_2)
    add_custom_command(
        OUTPUT ${COPIED_SRCS_2} ${COPIED_HEADERS_2}
        COMMAND cp ${COPIED_SRCS} ${REWRAPPED_DIR}/
        COMMAND mkdir -p ${REWRAPPED_INCLUDE_DIR}
        COMMAND cp ${COPIED_HEADERS} ${REWRAPPED_INCLUDE_DIR}
        DEPENDS ${COPIED_SRCS} ${COPIED_HEADERS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_custom_target(
        copy_files_2_${MAIN}
        DEPENDS ${COPIED_SRCS_2} ${COPIED_HEADERS_2}
    )
    add_dependencies(copy_files_2_${MAIN} ${TEST_NAME}_call_gate_generation)

    set(DYN_SYM ${libia2_BINARY_DIR}/dynsym.syms)

    # Define two targets per rewriter run. The targets that go into the compile
    # commands JSON are not intended to be built by CMake so we exclude them
    # from build-all.
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    add_executable(${MAIN} ${COPIED_SRCS})
    set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
    add_executable(${WRAPPED_MAIN} ${COPIED_SRCS})
    add_dependencies(${WRAPPED_MAIN} ${TEST_NAME}_call_gate_generation)
    set_target_properties(${MAIN} PROPERTIES EXCLUDE_FROM_ALL 1)

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    # We can't use the ${WRAPPED_MAIN} target here because the source paths
    # differ from the ${REWRAPPED_MAIN} paths
    add_executable(${WRAPPED_MAIN}_ccjson ${COPIED_SRCS_2})
    set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
    add_executable(${REWRAPPED_MAIN} ${COPIED_SRCS_2})
    add_dependencies(${REWRAPPED_MAIN} ${TEST_NAME}_call_gate_generation_2)
    set_target_properties(${WRAPPED_MAIN}_ccjson PROPERTIES EXCLUDE_FROM_ALL 1)

    # TODO: Some tests use this macro to simulate changes that need to be made
    # manually. Do we need this for the wrapped ccjson target?
    target_compile_definitions(${MAIN} PRIVATE PRE_REWRITER=1)
    # Define options common to all targets
    foreach(target ${MAIN} ${WRAPPED_MAIN} ${WRAPPED_MAIN}_ccjson ${REWRAPPED_MAIN})
        set_target_properties(${target} PROPERTIES PKEY ${DEFINE_TEST_PKEY})

        if(DEFINED DEFINE_TEST_PKEY)
            target_compile_definitions(${target} PRIVATE PKEY=${DEFINE_TEST_PKEY})
        endif()
        target_compile_options(${target} PRIVATE
            "-Werror=incompatible-pointer-types"
            "-fsanitize=undefined"
            "-fPIC"
            "-g")
        target_compile_options(${target} PRIVATE
            "-isystem${C_SYSTEM_INCLUDE}"
            "-isystem${C_SYSTEM_INCLUDE_FIXED}")
        target_link_options(${target} PRIVATE
            # UBSAN requires passing this as both a compiler and linker flag
            "-fsanitize=undefined"
            "-Wl,--export-dynamic")
        target_link_libraries(${target} PRIVATE
            dl
            libia2
            partition-alloc)
    endforeach()
    # Define options common to both targets for the first rewriter run
    foreach(target ${MAIN} ${WRAPPED_MAIN})
        if (${DEFINE_TEST_PKEY} GREATER 0)
            target_include_directories(${target} BEFORE PRIVATE ${REWRITTEN_INCLUDE_DIR})
        else()
            target_include_directories(${target} BEFORE PRIVATE ${ORIGINAL_INCLUDE_DIR})
        endif()
    endforeach()
    # Define options common to both targets for the second rewriter run
    foreach(target ${WRAPPED_MAIN}_ccjson ${REWRAPPED_MAIN})
        if (${DEFINE_TEST_PKEY} GREATER 0)
            target_include_directories(${target} BEFORE PRIVATE ${REWRAPPED_INCLUDE_DIR})
        else()
            target_include_directories(${target} BEFORE PRIVATE ${ORIGINAL_INCLUDE_DIR})
        endif()
    endforeach()
    add_dependencies(check-ia2 ${WRAPPED_MAIN} ${REWRAPPED_MAIN})

    # Define options common to the wrapped targets
    foreach(target ${WRAPPED_MAIN} ${REWRAPPED_MAIN})
        target_link_options(${target} PRIVATE
                "-Wl,-T${PADDING_LINKER_SCRIPT}"
                "-Wl,--dynamic-list=${DYN_SYM}")
        set_target_properties(${WRAPPED_MAIN} PROPERTIES LINK_DEPENDS ${PADDING_LINKER_SCRIPT})
    endforeach()
    target_link_libraries(${WRAPPED_MAIN} PRIVATE
        ${TEST_NAME}_call_gates)
    target_link_libraries(${REWRAPPED_MAIN} PRIVATE
        ${TEST_NAME}_call_gates_2)
    get_target_property(LIB_PKEY ${DEFINE_TEST_LIBS} PKEY)

    target_link_libraries(${MAIN} PRIVATE ${DEFINE_TEST_LIBS})
    target_link_libraries(${WRAPPED_MAIN}_ccjson PRIVATE ${DEFINE_TEST_LIBS})
    if (${LIB_PKEY} GREATER 0)
        target_link_libraries(${WRAPPED_MAIN} PRIVATE
            ${DEFINE_TEST_LIBS}_wrapped_padded)
        target_link_libraries(${REWRAPPED_MAIN} PRIVATE
            ${DEFINE_TEST_LIBS}_rewrapped_padded)
    else()
        target_link_libraries(${WRAPPED_MAIN} PRIVATE
            ${DEFINE_TEST_LIBS}_wrapped)
        target_link_libraries(${REWRAPPED_MAIN} PRIVATE
            ${DEFINE_TEST_LIBS}_rewrapped)
    endif()

    if (DEFINE_TEST_NEEDS_LD_WRAP)
        set_target_properties(${WRAPPED_MAIN} PROPERTIES NEEDS_LD_WRAP YES)
        target_link_options(${WRAPPED_MAIN} PRIVATE
            "-Wl,@${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates_${DEFINE_TEST_PKEY}.ld")
        target_link_options(${REWRAPPED_MAIN} PRIVATE
            "-Wl,@${REWRAPPED_DIR}/${TEST_NAME}_call_gates_${DEFINE_TEST_PKEY}.ld")
    else()
        set_target_properties(${WRAPPED_MAIN} PROPERTIES NEEDS_LD_WRAP NO)
    endif()
    if (${DEFINE_TEST_PKEY} GREATER 0)
        target_compile_options(${WRAPPED_MAIN} PRIVATE
            "-include${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates.h")
        target_compile_options(${REWRAPPED_MAIN} PRIVATE
            "-include${REWRAPPED_DIR}/${TEST_NAME}_call_gates.h")
    endif()

    # Find libc. We cannot simply do `find_library(LIBC c REQUIRED)` because
    # `libc.so` itself is a linker script with glibc, while we need the path of
    # the actual ELF shared object itself so we can patch its program headers.
    list(APPEND LIBC_PATHS /lib/libc.so.6 /lib64/libc.so.6 /lib/x86_64-linux-gnu/libc.so.6)
    foreach(CANDIDATE in ${LIBC_PATHS})
        if (EXISTS ${CANDIDATE})
            set(LIBC_PATH ${CANDIDATE})
        endif()
    endforeach()
    if(NOT DEFINED LIBC_PATH)
        string(REPLACE ";" "\t" PATHS "${LIBC_PATHS}")
        message(FATAL_ERROR
            "Could not find libc.so.6 in:\n${PATHS}\n")
    endif()

    # Generate libc with padded TLS segment
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libc.so.6
        COMMAND ${CMAKE_COMMAND} -E copy ${LIBC_PATH} ${CMAKE_CURRENT_BINARY_DIR}/libc.so.6
        COMMAND pad-tls --allow-no-tls ${CMAKE_CURRENT_BINARY_DIR}/libc.so.6
        DEPENDS pad-tls
        COMMENT "Padding TLS segment of libc"
    )

    add_custom_target(${TEST_NAME}-libc DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libc.so.6")

    # Use libc in wrapper
    add_dependencies(${WRAPPED_MAIN} ${TEST_NAME}-libc)
    add_dependencies(${REWRAPPED_MAIN} ${TEST_NAME}-libc)
endfunction()
