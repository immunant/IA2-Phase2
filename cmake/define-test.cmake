# Get system header directories for header rewriter as it needs them explicitly passed
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include
  OUTPUT_VARIABLE C_SYSTEM_INCLUDE
  OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include-fixed
  OUTPUT_VARIABLE C_SYSTEM_INCLUDE_FIXED
  OUTPUT_STRIP_TRAILING_WHITESPACE)

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
        set(ORIGINAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${SHARED_LIB_INCLUDE_DIR})
        set(REWRITTEN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/${SHARED_LIB_INCLUDE_DIR})
    else()
        set(RELATIVE_INCLUDE_DIR include)
        set(ORIGINAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set(REWRITTEN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
    endif()
    
    set(ORIGINAL_SRCS ${SHARED_LIB_SRCS})
    list(TRANSFORM ORIGINAL_SRCS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
    set(COPIED_SRCS ${SHARED_LIB_SRCS})
    list(TRANSFORM COPIED_SRCS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)

    if(DEFINED SHARED_LIB_INCLUDE_DIR)
        file(GLOB COPIED_LIB_HEADERS "${ORIGINAL_INCLUDE_DIR}/*.h")
        add_custom_target(copy_files_${LIBNAME}
            COMMAND cp ${SHARED_LIB_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/
            COMMAND mkdir -p ${REWRITTEN_INCLUDE_DIR}
            COMMAND cp ${COPIED_LIB_HEADERS} ${REWRITTEN_INCLUDE_DIR}/
            BYPRODUCTS ${COPIED_SRCS} ${COPIED_LIB_HEADERS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    else()
        add_custom_target(copy_files_${LIBNAME}
            COMMAND cp ${SHARED_LIB_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/
            BYPRODUCTS ${COPIED_SRCS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()

    set(PADDING_LINKER_SCRIPT ${libia2_BINARY_DIR}/padding.ld)

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

        add_dependencies(${target} libia2)

        if(LIBIA2_INSECURE)
            target_compile_definitions(${target} PUBLIC LIBIA2_INSECURE=1)
        endif()
        if (DEFINED SHARED_LIB_PKEY)
            target_compile_definitions(${target} PRIVATE PKEY=${SHARED_LIB_PKEY})
        endif()
        target_compile_definitions(${target} PRIVATE _GNU_SOURCE)
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
            "-Wl,-z,now"
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

    if (NOT DEFINED DEFINE_TEST_PKEY)
        set(DEFINE_TEST_PKEY "1")
    endif()

    if (NOT DEFINED DEFINE_TEST_LIBS)
        set(DEFINE_TEST_LIBS ${TEST_NAME}_lib)
    endif()

    if(DEFINED DEFINE_TEST_INCLUDE_DIR)
        set(RELATIVE_INCLUDE_DIR ${DEFINE_TEST_INCLUDE_DIR})
        set(ORIGINAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${DEFINE_TEST_INCLUDE_DIR})
        set(REWRITTEN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/${DEFINE_TEST_INCLUDE_DIR})
    else()
        set(RELATIVE_INCLUDE_DIR include)
        set(ORIGINAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set(REWRITTEN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
    endif()

    set(ORIGINAL_SRCS ${DEFINE_TEST_SRCS})
    list(TRANSFORM ORIGINAL_SRCS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
    set(COPIED_SRCS ${DEFINE_TEST_SRCS})
    list(TRANSFORM COPIED_SRCS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)

    file(GLOB COPIED_HEADERS "${ORIGINAL_INCLUDE_DIR}/*.h")
    add_custom_target(copy_files_${MAIN}
        COMMAND cp ${DEFINE_TEST_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/
        COMMAND mkdir -p ${REWRITTEN_INCLUDE_DIR}
        COMMAND cp ${COPIED_HEADERS} ${REWRITTEN_INCLUDE_DIR}/
        BYPRODUCTS ${COPIED_SRCS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    set(PADDING_LINKER_SCRIPT ${libia2_BINARY_DIR}/padding.ld)
    set(DYN_SYM ${libia2_BINARY_DIR}/dynsym.syms)

    # Define two targets
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    add_executable(${MAIN} ${COPIED_SRCS})
    set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
    add_executable(${WRAPPED_MAIN} ${COPIED_SRCS})
    add_dependencies(${WRAPPED_MAIN} ${TEST_NAME}_call_gate_generation)
    set_target_properties(${MAIN} PROPERTIES EXCLUDE_FROM_ALL 1)

    target_compile_definitions(${MAIN} PRIVATE PRE_REWRITER=1)
    # Define options common to both targets
    foreach(target ${MAIN} ${WRAPPED_MAIN})
        set_target_properties(${target} PROPERTIES PKEY ${DEFINE_TEST_PKEY})

        add_dependencies(${target} libia2)

        if(LIBIA2_INSECURE)
            target_compile_definitions(${target} PUBLIC LIBIA2_INSECURE=1)
        endif()
        if(DEFINED DEFINE_TEST_PKEY)
            target_compile_definitions(${target} PRIVATE PKEY=${DEFINE_TEST_PKEY})
        endif()
        target_compile_definitions(${target} PRIVATE _GNU_SOURCE)
        target_compile_options(${target} PRIVATE
            "-Werror=incompatible-pointer-types"
            "-fsanitize=undefined"
            "-fPIC"
            "-g")

        if (${DEFINE_TEST_PKEY} GREATER 0)
            target_include_directories(${target} BEFORE PRIVATE ${REWRITTEN_INCLUDE_DIR})
        else()
            target_include_directories(${target} BEFORE PRIVATE ${ORIGINAL_INCLUDE_DIR})
        endif()
        target_compile_options(${target} PRIVATE
            "-isystem${C_SYSTEM_INCLUDE}"
            "-isystem${C_SYSTEM_INCLUDE_FIXED}")
        target_link_options(${target} PRIVATE
            "-Wl,-z,now"
            # UBSAN requires passing this as both a compiler and linker flag
            "-fsanitize=undefined"
            "-Wl,--export-dynamic")
        target_link_libraries(${target} PRIVATE
            dl
            libia2
            partition-alloc)
    endforeach()
    add_dependencies(check-ia2 ${WRAPPED_MAIN})

    target_link_libraries(${MAIN} PRIVATE ${DEFINE_TEST_LIBS})

    target_link_options(${WRAPPED_MAIN} PRIVATE
            "-Wl,-T${PADDING_LINKER_SCRIPT}"
            "-Wl,--dynamic-list=${DYN_SYM}")
    target_link_libraries(${WRAPPED_MAIN} PRIVATE
        ${TEST_NAME}_call_gates)
    get_target_property(LIB_PKEY ${DEFINE_TEST_LIBS} PKEY)

    target_link_libraries(${MAIN} PRIVATE ${DEFINE_TEST_LIBS})
    if (${LIB_PKEY} GREATER 0)
        target_link_libraries(${WRAPPED_MAIN} PRIVATE
            ${DEFINE_TEST_LIBS}_wrapped_padded)
    else()
        target_link_libraries(${WRAPPED_MAIN} PRIVATE
            ${DEFINE_TEST_LIBS}_wrapped)
    endif()

    if (DEFINE_TEST_NEEDS_LD_WRAP)
        set_target_properties(${WRAPPED_MAIN} PROPERTIES NEEDS_LD_WRAP YES)
        target_link_options(${WRAPPED_MAIN} PRIVATE
            "-Wl,@${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates_${DEFINE_TEST_PKEY}.ld")
    else()
        set_target_properties(${WRAPPED_MAIN} PROPERTIES NEEDS_LD_WRAP NO)
    endif()
    if (${DEFINE_TEST_PKEY} GREATER 0)
        target_compile_options(${WRAPPED_MAIN} PRIVATE
            "-include${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates.h")
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
endfunction()
