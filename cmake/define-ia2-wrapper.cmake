# We need to get the system headers specifically from clang
find_program(CLANG_EXE
  clang
  PATHS /usr/bin/clang
  DOC "Path to clang compiler")

if(NOT CLANG_EXE)
  message(FATAL_ERROR "Could not find Clang, please set CLANG_EXE manually")
endif()
message(STATUS "Found Clang executable: ${CLANG_EXE}")

# Get system header directories for header rewriter as it needs them explicitly passed
execute_process(COMMAND ${CLANG_EXE} -print-file-name=include
  OUTPUT_VARIABLE CLANG_HEADERS_INCLUDE
  OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "Found Clang headers: ${CLANG_HEADERS_INCLUDE}")

execute_process(COMMAND ${CLANG_EXE} -print-file-name=include-fixed
  OUTPUT_VARIABLE CLANG_HEADERS_INCLUDE_FIXED
  OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "Found Clang fixed headers: ${CLANG_HEADERS_INCLUDE_FIXED}")

# Creates a target for the shared library with call gate wrappers for a given test
#
# ORIGINAL_TARGETS (optional) - all targets for the non-compartmentalized build.
#       Defaults to ${TEST_NAME}_main and ${TEST_NAME}_lib which are the default
#       target names set by define_shared_lib and define_test.
# EXTRA_REWRITER_ARGS (optional) - a list of extra arguments for the rewriter.
function(define_ia2_wrapper)
    # Parse options
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs ORIGINAL_TARGETS EXTRA_REWRITER_ARGS)
    cmake_parse_arguments(DEFINE_IA2_WRAPPER "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN} )

    get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

    if (NOT DEFINED DEFINE_IA2_WRAPPER_ORIGINAL_TARGETS)
        set(DEFINE_IA2_WRAPPER_ORIGINAL_TARGETS ${TEST_NAME}_lib ${TEST_NAME}_main)
    endif()

    set(ALL_SRCS "")
    set(LD_ARGS_FILES "")
    foreach(target ${DEFINE_IA2_WRAPPER_ORIGINAL_TARGETS})
        get_target_property(SRCS ${target}_wrapped SOURCES)
        list(APPEND ALL_SRCS ${SRCS})

        get_target_property(GENERATES_LD_WRAP ${target}_wrapped NEEDS_LD_WRAP)
        if (GENERATES_LD_WRAP)
            get_target_property(PKEY ${target}_wrapped PKEY)
            list(APPEND LD_ARGS_FILES ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates_${PKEY}.ld)
        endif()
    endforeach()
    set(ORIGINAL_SRCS ${ALL_SRCS})
    list(TRANSFORM ORIGINAL_SRCS REPLACE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

    set(CALL_GATE_SRC ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates.c)
    set(CALL_GATE_HDR ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates.h)

    add_custom_command(
        OUTPUT ${CALL_GATE_SRC} ${CALL_GATE_HDR} ${LD_ARGS_FILES}
               ${ALL_SRCS}
        COMMAND ia2-rewriter
            --output-prefix=${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates
            --root-directory=${CMAKE_CURRENT_SOURCE_DIR}
            --output-directory=${CMAKE_CURRENT_BINARY_DIR}
            # Set the build path so the rewriter can find the compile_commands JSON
            -p=${CMAKE_BINARY_DIR}
            --extra-arg=-isystem "--extra-arg=${CLANG_HEADERS_INCLUDE}"
            --extra-arg=-isystem "--extra-arg=${CLANG_HEADERS_INCLUDE_FIXED}"
            ${DEFINE_IA2_WRAPPER_EXTRA_REWRITER_ARGS}
            ${ORIGINAL_SRCS}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS ia2-rewriter ${ORIGINAL_SRCS}
        VERBATIM)

    add_custom_target(
        ${TEST_NAME}_call_gate_generation
        DEPENDS ${CALL_GATE_SRC} ${CALL_GATE_HDR} ${LD_ARGS_FILES}
    )

    set(CALL_GATE_TARGET ${TEST_NAME}_call_gates)
    add_library(${CALL_GATE_TARGET} SHARED ${CALL_GATE_SRC})
    if(LIBIA2_DEBUG)
        target_compile_definitions(${CALL_GATE_TARGET} PUBLIC LIBIA2_DEBUG=1)
    endif()
    target_compile_definitions(${CALL_GATE_TARGET} PRIVATE _GNU_SOURCE)
    target_link_libraries(${CALL_GATE_TARGET} PRIVATE libia2)
endfunction()
