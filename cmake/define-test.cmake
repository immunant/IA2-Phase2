# Defines a shared library target.
#
# Disables lazy binding (see issue #44).
#
# SRCS - source files for shared library
# LIBNAME - target name (defaults to ${TEST_NAME}-original
# INCLUDE_DIR - directories added to the search path (defaults to SRC_DIR/include)
# LINK_LIBS - additional libraries to link against (e.g. another lib's wrapper)
function(define_shared_lib)
    set(options "")
    set(oneValueArgs LIBNAME)
    set(multiValueArgs SRCS LINK_LIBS INCLUDE_DIR)
    cmake_parse_arguments(SHARED_LIB "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})
    get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    if(DEFINED SHARED_LIB_LIBNAME)
        set(LIBNAME ${SHARED_LIB_LIBNAME})
    else()
        set(LIBNAME ${TEST_NAME}-original)
    endif()
    if(DEFINED SHARED_LIB_INCLUDE_DIR)
        set(HEADER_DIR ${SHARED_LIB_INCLUDE_DIR})
    else()
        set(HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/)
    endif()

    add_library(${LIBNAME} SHARED ${SHARED_LIB_SRCS})
    target_include_directories(${LIBNAME} BEFORE PRIVATE
        ${HEADER_DIR}
        # Add top-level include directory for segfault handler
        ${IA2_INCLUDE_DIR})
    target_link_options(${LIBNAME} PRIVATE "-Wl,-z,now")
    target_link_libraries(${LIBNAME} PRIVATE
        ${SHARED_LIB_LINK_LIBS})
endfunction()


# Defines an executable target for a test.
#
# Disables lazy binding (see issue #44) and page-aligns the executable's
# segments. Unconditionally links against libia2.so and uses
# -Werror=incompatible-pointer-types.
#
# SRCS - source files for the executable
# WRAPPERS - Additional libraries to link against.
# COMPILE_OPTS - compile options for the executable
# INCLUDE_DIR - directories added to the search path (defaults to SRC_DIR/include)
function(define_test)
    # Parse options
    set(options)
    set(oneValueArgs "")
    set(multiValueArgs SRCS WRAPPERS COMPILE_OPTS INCLUDE_DIR)
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
    if(DEFINED DEFINE_TEST_INCLUDE_DIR)
        set(HEADER_DIR ${DEFINE_TEST_INCLUDE_DIR})
    else()
        set(HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/)
    endif()

    set(LINKER_SCRIPT ${libia2_BINARY_DIR}/padding.ld)
    add_executable(${MAIN} ${DEFINE_TEST_SRCS})
    target_compile_options(${MAIN} PRIVATE "-Werror=incompatible-pointer-types"
        ${DEFINE_TEST_COMPILE_OPTS})
    target_link_options(${MAIN} PRIVATE "-Wl,-z,now" "-Wl,-T${LINKER_SCRIPT}")
    target_include_directories(${MAIN} BEFORE PRIVATE
        ${HEADER_DIR}
        # Add top-level include directory for segfault handler
        ${IA2_INCLUDE_DIR})
    target_link_libraries(${MAIN} PRIVATE
        ${WRAPPERS}
        ${IA2_LIB})
    add_dependencies(check-ia2 ${MAIN})
endfunction()
