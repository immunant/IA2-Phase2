# Defines a shared library target.
#
# Disables lazy binding (see issue #44). Unconditionally uses -fPIC.
#
# SRCS - source files for shared library
# LIBNAME - target name (defaults to ${TEST_NAME}-original
# INCLUDE_DIR - directories added to the search path (defaults to SRC_DIR/include)
# LINK_LIBS - additional libraries to link against (e.g. another lib's wrapper)
function(define_shared_lib)
    set(options "")
    set(oneValueArgs LIBNAME)
    set(multiValueArgs SRCS LINK_LIBS LINK_OPTS INCLUDE_DIR)
    cmake_parse_arguments(SHARED_LIB "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})
    get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    if(DEFINED SHARED_LIB_LIBNAME)
        set(LIBNAME ${SHARED_LIB_LIBNAME})
    else()
        set(LIBNAME ${TEST_NAME}-original)
    endif()
    if(DEFINED SHARED_LIB_INCLUDE_DIR)
        set(INCLUDE_DIR ${SHARED_LIB_INCLUDE_DIR})
    else()
        set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/)
    endif()

    add_library(${LIBNAME} SHARED ${SHARED_LIB_SRCS})
    if(LIBIA2_INSECURE)
        target_compile_definitions(${LIBNAME} PUBLIC LIBIA2_INSECURE=1)
    endif()
    target_compile_options(${LIBNAME} PRIVATE "-fPIC")
    target_include_directories(${LIBNAME} BEFORE PRIVATE
        ${INCLUDE_DIR}
        # Add top-level include directory for segfault handler
        ${IA2_INCLUDE_DIR})
    target_link_options(${LIBNAME} PRIVATE "-Wl,-z,now"
        ${SHARED_LIB_LINK_OPTS})
    target_link_libraries(${LIBNAME} PRIVATE
        ${SHARED_LIB_LINK_LIBS})
endfunction()


# Defines an executable target for a test.
#
# Disables lazy binding (see issue #44) and page-aligns the executable's
# segments. Unconditionally uses -Werror=incompatible-pointer-types and -fPIC.
#
# SRCS - source files for the executable
# WRAPPERS - Additional libraries to link against.
# COMPILE_OPTS - compile options for the executable
# INCLUDE_DIR - directories added to the search path (defaults to BIN_DIR).
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
        set(INCLUDE_DIR ${DEFINE_TEST_INCLUDE_DIR})
    else()
        set(INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    set(LINKER_SCRIPT ${IA2_LINKER_SCRIPT})
    set(DYN_SYM ${IA2_DYNSYMS})
    add_executable(${MAIN} ${DEFINE_TEST_SRCS})
    set_target_properties(${MAIN} PROPERTIES LINK_DEPENDS "${LINKER_SCRIPT};${DYN_SYM}")
    if(LIBIA2_INSECURE)
        target_compile_definitions(${MAIN} PUBLIC LIBIA2_INSECURE=1)
    endif()
    target_compile_options(${MAIN} PRIVATE
        "-Werror=incompatible-pointer-types"
        "-fPIC"
        ${DEFINE_TEST_COMPILE_OPTS})
    target_link_options(${MAIN} PRIVATE "-Wl,-z,now" "-Wl,-T${LINKER_SCRIPT}" "-Wl,--dynamic-list=${DYN_SYM}")
    target_include_directories(${MAIN} BEFORE PRIVATE
        ${INCLUDE_DIR}
        # Add top-level include directory for segfault handler
        ${IA2_INCLUDE_DIR})
    target_link_libraries(${MAIN} PRIVATE
        ${WRAPPERS})
    add_dependencies(check-ia2 ${MAIN})
endfunction()
