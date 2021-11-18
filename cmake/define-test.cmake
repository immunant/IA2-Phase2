# Defines a shared library target from the source file arguments following
# `SRCS` with the linker options required for compartmentalization. Adds the
# argument following `INCLUDE_DIR` to the search path (defaults to
# `test_dir/include`). The target is named `LIBNAME` (defaults to
# `${TEST_NAME}-original`)
function(define_shared_lib)
    set(options "")
    set(oneValueArgs LIBNAME INCLUDE_DIR)
    set(multiValueArgs SRCS)
    cmake_parse_arguments(SHARED_LIB "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})
    get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    if(DEFINED SHARED_LIB_LIBNAME)
        set(LIBNAME ${SHARED_LIB_LIBNAME})
    else()
        set(LIBNAME ${TEST_NAME}-original)
    endif()
    if(DEFINED SHARED_LIB_INCLUDE_DIR)
        set(ORIGINAL_HEADER_DIR ${SHARED_LIB_INCLUDE_DIR})
    else()
        set(ORIGINAL_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/)
    endif()

    add_library(${LIBNAME} SHARED ${SHARED_LIB_SRCS})
    target_include_directories(${LIBNAME} BEFORE PRIVATE ${ORIGINAL_HEADER_DIR})
    target_link_options(${LIBNAME} PRIVATE "-Wl,-z,now")
endfunction()


# Defines an executable target for a test using the source file arguments
# following `SRCS`. Links against the default wrapper library name or the
# wrapper target arguments following `WRAPPERS`.
function(define_test)
    # Parse options
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SRCS WRAPPERS COMPILE_OPTS)
    cmake_parse_arguments(DEFINE_TEST "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

    # Set executable and wrapper target names
    get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    set(MAIN ${TEST_NAME}-main)
    if(DEFINED DEFINE_TEST_WRAPPERS)
        set(WRAPPERS ${DEFINE_TEST_WRAPPERS})
    else()
        set(WRAPPERS ${TEST_NAME}-wrapper)
    endif()

    set(LINKER_SCRIPT ${libia2_BINARY_DIR}/padding.ld)
    add_executable(${MAIN} ${DEFINE_TEST_SRCS})
    target_compile_options(${MAIN} PRIVATE ${DEFINE_TEST_COMPILE_OPTS})
    target_link_options(${MAIN} PRIVATE "-Wl,-z,now" "-Wl,-T${LINKER_SCRIPT}")
    target_link_libraries(${MAIN} PRIVATE
        ${WRAPPERS}
        ${IA2_LIB})
    add_dependencies(check-ia2 ${MAIN})
endfunction()
