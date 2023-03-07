include(CMakePrintHelpers)
# Creates a target that for the shared library with call gate wrappers for a given test
#
# ORIGINAL_TARGETS (optional) - all targets for the non-compartmentalized build.
#       Defaults to ${TEST_NAME}_main and ${TEST_NAME}_lib which are the default
#       target names set by define_shared_lib and define_test.
function(define_ia2_wrapper)
    # Parse options
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs ORIGINAL_TARGETS)
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
    set(COPY_TARGETS ${DEFINE_IA2_WRAPPER_ORIGINAL_TARGETS})
    list(TRANSFORM COPY_TARGETS PREPEND "copy_files_")

    set(CALL_GATE_SRC ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates.c)
    set(CALL_GATE_HDR ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates.h)

    add_custom_target(
        ${TEST_NAME}_call_gate_generation
        BYPRODUCTS ${CALL_GATE_SRC} ${CALL_GATE_HDR} ${LD_ARGS_FILES}
        COMMAND ia2-header-rewriter
            --extra-arg=-DPRE_REWRITER
            --output-prefix=${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_call_gates
            ${ALL_SRCS}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS ia2-header-rewriter ${COPY_TARGETS}
        VERBATIM)

    set(CALL_GATE_TARGET ${TEST_NAME}_call_gates)
    add_library(${CALL_GATE_TARGET} SHARED ${CALL_GATE_SRC})
    if(LIBIA2_INSECURE)
        target_compile_definitions(${CALL_GATE_TARGET} PUBLIC LIBIA2_INSECURE=1)
    endif()
    target_compile_definitions(${CALL_GATE_TARGET} PRIVATE _GNU_SOURCE)
    target_link_libraries(${CALL_GATE_TARGET} PRIVATE libia2)
endfunction()
