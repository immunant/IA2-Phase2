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
    set(options NEEDS_LD_WRAP NO_UBSAN)
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

    if (NOT DEFINED SHARED_LIB_PKEY)
        set(SHARED_LIB_PKEY "0")
    endif()

    # INCLUDE_DIR is relative to the test target directory
    if(DEFINED SHARED_LIB_INCLUDE_DIR)
        set(RELATIVE_INCLUDE_DIR ${SHARED_LIB_INCLUDE_DIR})
    else()
        set(RELATIVE_INCLUDE_DIR include)
    endif()

    if(SHARED_LIB_NO_UBSAN)
        set(ENABLE_UBSAN "")
    else()
        set(ENABLE_UBSAN ENABLE_UBSAN)
    endif()

    add_ia2_compartment(${LIBNAME} LIBRARY ${ENABLE_UBSAN}
        PKEY ${SHARED_LIB_PKEY}
        LIBRARIES ${SHARED_LIB_UNWRAPPED_LIBS}
        SOURCES ${SHARED_LIB_SRCS}
        INCLUDE_DIRECTORIES ${SHARED_LIB_UNWRAPPED_INCLUDE_DIRS} ${RELATIVE_INCLUDE_DIR}
    )

    target_include_directories(${LIBNAME} PUBLIC
        ${SHARED_LIB_UNWRAPPED_INCLUDE_DIRS})
    target_link_directories(${LIBNAME} PUBLIC
        ${SHARED_LIB_UNWRAPPED_LIBRARY_DIRS})
    target_link_libraries(${LIBNAME} PUBLIC
        ${SHARED_LIB_UNWRAPPED_LIBS})
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
    set(options NEEDS_LD_WRAP NOT_IN_CHECK_IA2 NO_LIBS NO_UBSAN CRITERION_TEST WITHOUT_SANDBOX)
    set(oneValueArgs PKEY NAME)
    set(multiValueArgs LIBS SRCS INCLUDE_DIR
        UNWRAPPED_INCLUDE_DIRS UNWRAPPED_LIBRARY_DIRS UNWRAPPED_LIBS)
    cmake_parse_arguments(DEFINE_TEST "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

    if (DEFINED DEFINE_TEST_NAME)
        set(TEST_NAME ${DEFINE_TEST_NAME})
    else()
        # Set executable and wrapper target names
        get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    endif()

    if (NOT DEFINED DEFINE_TEST_PKEY)
        set(DEFINE_TEST_PKEY "1")
    endif()

    if (NOT DEFINED DEFINE_TEST_LIBS AND NOT DEFINE_TEST_NO_LIBS)
        set(DEFINE_TEST_LIBS ${TEST_NAME}_lib)
    endif()

    if (LIBIA2_AARCH64)
        set(DEFINE_TEST_WITHOUT_SANDBOX TRUE)
    endif()

#    if(DEFINED DEFINE_TEST_INCLUDE_DIR)
#        set(RELATIVE_INCLUDE_DIR ${DEFINE_TEST_INCLUDE_DIR})
#    else()
#        set(RELATIVE_INCLUDE_DIR include)
#    endif()
#    set(ORIGINAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${RELATIVE_INCLUDE_DIR})
#    set(REWRITTEN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/${RELATIVE_INCLUDE_DIR})
#
#    set(ORIGINAL_SRCS ${DEFINE_TEST_SRCS})
#    list(TRANSFORM ORIGINAL_SRCS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
#    set(COPIED_SRCS ${DEFINE_TEST_SRCS})
#    list(TRANSFORM COPIED_SRCS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)
#
#    set(DYN_SYM ${libia2_BINARY_DIR}/dynsym.syms)
#
#    # Define two targets
#    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#    add_executable(${MAIN} ${ORIGINAL_SRCS})
#    set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
#    add_executable(${WRAPPED_MAIN} ${COPIED_SRCS})
#    add_dependencies(${WRAPPED_MAIN} ${TEST_NAME}_call_gate_generation)
#    set_target_properties(${MAIN} PROPERTIES EXCLUDE_FROM_ALL 1)

    set(ADD_SANDBOX_DEP FALSE)
    if (DEFINE_TEST_CRITERION_TEST)
        list(APPEND DEFINE_TEST_UNWRAPPED_LIBS criterion)
        if (NOT DEFINE_TEST_NOT_IN_CHECK_IA2)
            if (LIBIA2_AARCH64)
                add_test(NAME ${TEST_NAME}
                    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
                        "-one-insn-per-tb"
                        "-L" "${CMAKE_BINARY_DIR}/external/glibc/sysroot/usr/"
                        ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
            elseif (DEFINE_TEST_WITHOUT_SANDBOX OR NOT ${IA2_TRACER})
                add_test(${TEST_NAME} ${TEST_NAME})
            else()
                add_test(NAME ${TEST_NAME} COMMAND ${CMAKE_BINARY_DIR}/runtime/tracer/ia2-sandbox ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
                set(ADD_SANDBOX_DEP TRUE)
            endif()
            add_dependencies(check ${TEST_NAME})
        endif()
    endif()

    if(DEFINED DEFINE_TEST_INCLUDE_DIR)
        set(RELATIVE_INCLUDE_DIR ${DEFINE_TEST_INCLUDE_DIR})
    else()
        set(RELATIVE_INCLUDE_DIR include)
    endif()

    if (NOT DEFINE_TEST_NOT_IN_CHECK_IA2)
        add_dependencies(check-ia2 ${TEST_NAME})
    endif()

    if(SHARED_LIB_NO_UBSAN)
        set(ENABLE_UBSAN "")
    else()
        set(ENABLE_UBSAN ENABLE_UBSAN)
    endif()

    add_ia2_compartment(${TEST_NAME} EXECUTABLE ${ENABLE_UBSAN}
        PKEY ${DEFINE_TEST_PKEY}
        LIBRARIES ${DEFINE_TEST_UNWRAPPED_LIBS} ${DEFINE_TEST_LIBS}
        SOURCES ${DEFINE_TEST_SRCS}
        INCLUDE_DIRECTORIES ${DEFINE_TEST_UNWRAPPED_INCLUDE_DIRS} ${RELATIVE_INCLUDE_DIR}
    )

    if(ADD_SANDBOX_DEP)
        add_dependencies(${TEST_NAME} ia2-sandbox)
    endif()

    target_include_directories(${TEST_NAME} PUBLIC
        ${SHARED_LIB_UNWRAPPED_INCLUDE_DIRS})
    target_link_directories(${TEST_NAME} PUBLIC
        ${SHARED_LIB_UNWRAPPED_LIBRARY_DIRS})
    target_link_libraries(${TEST_NAME} PUBLIC
        ${SHARED_LIB_UNWRAPPED_LIBS})
endfunction()

function(define_ia2_main)
    define_test(${ARGV} NOT_IN_CHECK_IA2)
endfunction()
