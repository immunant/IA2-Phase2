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
# UNWRAPPED_INCLUDE_DIRS (optional) - extra directories with unwrapped headers.
# UNWRAPPED_LIBRARY_DIRS (optional) - extra directories with unwrapped headers.
# UNWRAPPED_LIBS (optional) - extra unwrapped libraries that this library links
#       against.
function(define_shared_lib)
    set(options NEEDS_LD_WRAP)
    set(oneValueArgs LIBNAME PKEY)
    set(multiValueArgs SRCS INCLUDE_DIR UNWRAPPED_INCLUDE_DIRS
        UNWRAPPED_LIBRARY_DIRS UNWRAPPED_LIBS)
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

    # Define two targets
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    add_library(${LIBNAME} SHARED ${ORIGINAL_SRCS})
    set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
    add_library(${WRAPPED_LIBNAME} SHARED ${COPIED_SRCS})
    add_dependencies(${WRAPPED_LIBNAME} ${TEST_NAME}_call_gate_generation)
    set_target_properties(${LIBNAME} PROPERTIES EXCLUDE_FROM_ALL 1)
    if (${SHARED_LIB_PKEY} GREATER 0)
        set_target_properties(${WRAPPED_LIBNAME} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/unaligned)
    endif()

    target_compile_definitions(${LIBNAME} PRIVATE IA2_ENABLE=0)
    target_compile_definitions(${WRAPPED_LIBNAME} PRIVATE IA2_ENABLE=1)
    target_include_directories(${LIBNAME} BEFORE PRIVATE ${ORIGINAL_INCLUDE_DIR})
    if (${SHARED_LIB_PKEY} GREATER 0)
        target_include_directories(${WRAPPED_LIBNAME} BEFORE PRIVATE ${REWRITTEN_INCLUDE_DIR})
    else()
        target_include_directories(${WRAPPED_LIBNAME} BEFORE PRIVATE ${ORIGINAL_INCLUDE_DIR})
    endif()
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

        target_link_options(${target} PRIVATE
            # UBSAN requires passing this as both a compiler and linker flag
            "-fsanitize=undefined")
        target_link_libraries(${target} PRIVATE
            partition-alloc
            libia2)

        target_include_directories(${target} PUBLIC
            ${SHARED_LIB_UNWRAPPED_INCLUDE_DIRS})
        target_link_directories(${target} PUBLIC
            ${SHARED_LIB_UNWRAPPED_LIBRARY_DIRS})
        target_link_libraries(${target} PUBLIC
            ${SHARED_LIB_UNWRAPPED_LIBS})
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
            OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
endfunction()


# Creates wrapped and unmodified executable targets for a test
#

# NEEDS_LD_WRAP - If present pass -Wl,@$LD_ARGS_FILE to the wrapped
#       library. The name of the file is generated from the target name and pkey.
# NOT_IN_CHECK_IA2 - If present, skip adding this target to check-ia2.
# PKEY (optional) - The pkey for the wrapped library (defaults to 1).
# LIBS (optional) - libraries to link the unmodified build against (defaults to
#       ${TEST_NAME}_lib which is the default set by define_shared_lib). If
#       NO_LIBS is set, no libraries will be used.
# SRCS (required) - The set of source files
# INCLUDE_DIR (optional) - include directory used to build the executable. If
#       specified, this must be relative to the source directory for the test.
#       Defaults to include/. If headers are also used for shared libraries, this
#       must not be specified.
# UNWRAPPED_INCLUDE_DIRS (optional) - extra include directories that are not
#       wrapped.
# UNWRAPPED_LIBRARY_DIRS (optional) - extra library directories.
# UNWRAPPED_LIBS (optional) - extra libraries that are not wrapped.
# CRITERION_TEST - If present, link against criterion and add the test to the
#                  cmake test infrastructure.
# WITHOUT_SANDBOX - If present, test is run without the IA2 sandbox runtime.
function(define_test)
    # Parse options
    set(options NEEDS_LD_WRAP NOT_IN_CHECK_IA2 NO_LIBS CRITERION_TEST WITHOUT_SANDBOX)
    set(oneValueArgs PKEY)
    set(multiValueArgs LIBS SRCS INCLUDE_DIR
        UNWRAPPED_INCLUDE_DIRS UNWRAPPED_LIBRARY_DIRS UNWRAPPED_LIBS)
    cmake_parse_arguments(DEFINE_TEST "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

    # Set executable and wrapper target names
    get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    set(MAIN ${TEST_NAME}_main)
    set(WRAPPED_MAIN ${TEST_NAME}_main_wrapped)

    if (NOT DEFINED DEFINE_TEST_PKEY)
        set(DEFINE_TEST_PKEY "1")
    endif()

    if (NOT DEFINED DEFINE_TEST_LIBS AND NOT DEFINE_TEST_NO_LIBS)
        set(DEFINE_TEST_LIBS ${TEST_NAME}_lib)
    endif()

    if(DEFINED DEFINE_TEST_INCLUDE_DIR)
        set(RELATIVE_INCLUDE_DIR ${DEFINE_TEST_INCLUDE_DIR})
    else()
        set(RELATIVE_INCLUDE_DIR include)
    endif()
    set(ORIGINAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${RELATIVE_INCLUDE_DIR})
    set(REWRITTEN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/${RELATIVE_INCLUDE_DIR})

    set(ORIGINAL_SRCS ${DEFINE_TEST_SRCS})
    list(TRANSFORM ORIGINAL_SRCS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
    set(COPIED_SRCS ${DEFINE_TEST_SRCS})
    list(TRANSFORM COPIED_SRCS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)

    set(DYN_SYM ${libia2_BINARY_DIR}/dynsym.syms)

    # Define two targets
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    add_executable(${MAIN} ${ORIGINAL_SRCS})
    set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
    add_executable(${WRAPPED_MAIN} ${COPIED_SRCS})
    add_dependencies(${WRAPPED_MAIN} ${TEST_NAME}_call_gate_generation)
    set_target_properties(${MAIN} PROPERTIES EXCLUDE_FROM_ALL 1)

    if (DEFINE_TEST_CRITERION_TEST)
        list(APPEND DEFINE_TEST_UNWRAPPED_LIBS criterion)
        if (NOT DEFINE_TEST_NOT_IN_CHECK_IA2)
            if (DEFINE_TEST_WITHOUT_SANDBOX)
                add_test(${TEST_NAME} ${WRAPPED_MAIN})
            else()
                add_test(NAME ${TEST_NAME} COMMAND ${CMAKE_BINARY_DIR}/runtime/ia2-sandbox ${CMAKE_CURRENT_BINARY_DIR}/${WRAPPED_MAIN} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
                add_dependencies(${WRAPPED_MAIN} ia2-sandbox)
            endif()
            add_dependencies(check ${WRAPPED_MAIN})
        endif()
    endif()

    target_compile_definitions(${MAIN} PRIVATE IA2_ENABLE=0)
    target_compile_definitions(${WRAPPED_MAIN} PRIVATE IA2_ENABLE=1)
    target_include_directories(${MAIN} BEFORE PRIVATE ${ORIGINAL_INCLUDE_DIR})
    if (${DEFINE_TEST_PKEY} GREATER 0)
        target_include_directories(${WRAPPED_MAIN} BEFORE PRIVATE ${REWRITTEN_INCLUDE_DIR})
    else()
        target_include_directories(${WRAPPED_MAIN} BEFORE PRIVATE ${ORIGINAL_INCLUDE_DIR})
    endif()
    # Define options common to both targets
    foreach(target ${MAIN} ${WRAPPED_MAIN})
        set_target_properties(${target} PROPERTIES PKEY ${DEFINE_TEST_PKEY})

        if(DEFINED DEFINE_TEST_PKEY)
            target_compile_definitions(${target} PRIVATE PKEY=${DEFINE_TEST_PKEY})
        endif()
        target_compile_options(${target} PRIVATE
            "-Werror=incompatible-pointer-types"
            "-fsanitize=undefined"
            "-fPIC"
            "-g")

        target_link_options(${target} PRIVATE
            # UBSAN requires passing this as both a compiler and linker flag
            "-fsanitize=undefined"
            "-Wl,--export-dynamic")
        target_include_directories(${target} PRIVATE
            ${DEFINE_TEST_UNWRAPPED_INCLUDE_DIRS})
        target_link_directories(${target} PRIVATE
            ${DEFINE_TEST_UNWRAPPED_LIBRARY_DIRS})
        target_link_libraries(${target} PRIVATE
            dl
            libia2
            partition-alloc
            ${DEFINE_TEST_UNWRAPPED_LIBS})
    endforeach()

    if (NOT DEFINE_TEST_NOT_IN_CHECK_IA2)
        add_dependencies(check-ia2 ${WRAPPED_MAIN})
    endif()

    target_link_libraries(${MAIN} PRIVATE ${DEFINE_TEST_LIBS})

    target_link_options(${WRAPPED_MAIN} PRIVATE
            "-Wl,-T${PADDING_LINKER_SCRIPT}"
            "-Wl,--dynamic-list=${DYN_SYM}")
    set_target_properties(${WRAPPED_MAIN} PROPERTIES LINK_DEPENDS ${PADDING_LINKER_SCRIPT})
    target_link_libraries(${WRAPPED_MAIN} PRIVATE
        ${TEST_NAME}_call_gates)
    if (DEFINED DEFINE_TEST_LIBS)
        get_target_property(LIB_PKEY ${DEFINE_TEST_LIBS} PKEY)
    else()
        set(LIB_PKEY 0)
    endif()

    if (DEFINED DEFINE_TEST_LIBS)
        target_link_libraries(${MAIN} PRIVATE ${DEFINE_TEST_LIBS})
        if (${LIB_PKEY} GREATER 0)
            target_link_libraries(${WRAPPED_MAIN} PRIVATE
                ${DEFINE_TEST_LIBS}_wrapped-padded)
        else()
            target_link_libraries(${WRAPPED_MAIN} PRIVATE
                ${DEFINE_TEST_LIBS}_wrapped)
        endif()
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
