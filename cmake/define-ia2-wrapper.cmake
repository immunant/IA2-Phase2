# Get system header directories for header rewriter as it needs them explicitly passed
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include
  OUTPUT_VARIABLE C_SYSTEM_INCLUDE
  OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include-fixed
  OUTPUT_VARIABLE C_SYSTEM_INCLUDE_FIXED
  OUTPUT_STRIP_TRAILING_WHITESPACE)

# Define an IA2 shim library for direct call wrappers.
#
# This runs the rewriter on the headers exported by the library that will be
# wrapped. Public headers are those that can be explicitly `#include`d by source
# files and private headers are those that are used internally (i.e. require
# context to be parsed. see issue #4).
#
# SRCS
# USE_SYSTEM_HEADERS - Use headers for library installed on the system
# WRAP_MAIN - Creates a shim for the main binary and changes the default target
#             name.
# WRAPPER - Wrapper target name. Defaults to ${TEST_NAME}-wrapper or
#           ${TEST_NAME}-main-wrapper.
# WRAPPED_LIB - Target name for library to wrap.Defaults to ${TEST_NAME}-original or
#               ${TEST_NAME}-main.
# INCLUDE_DIR - Added to search path in rewriter invocation. Defaults to
#               SRC_DIR/include.
# OUTPUT_DIR - Output directory relative to BIN_DIR to create and use for
#              rewritten headers. Defaults to BIN_DIR.
# CALLER_PKEY - Optional protection key for wrapper's caller. This is required
# TARGET_PKEY - Protection key for the wrapper's target, if any.
function(define_ia2_wrapper)
    # Parse options
    set(options WRAP_MAIN)
    set(oneValueArgs WRAPPER WRAPPED_LIB INCLUDE_DIR OUTPUT_DIR
        CALLER_PKEY TARGET_PKEY)
    set(multiValueArgs SRCS EXTRA_REWRITER_ARGS)
    cmake_parse_arguments(DEFINE_IA2_WRAPPER "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN} )

    # Shorter names for parsed args
    get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    if(DEFINED DEFINE_IA2_WRAPPER_WRAPPER)
        set(WRAPPER_TARGET ${DEFINE_IA2_WRAPPER_WRAPPER})
    elseif(DEFINE_IA2_WRAPPER_WRAP_MAIN)
        set(WRAPPER_TARGET ${TEST_NAME}-main-wrapper)
    else()
        set(WRAPPER_TARGET ${TEST_NAME}-wrapper)
    endif()
    if(DEFINED DEFINE_IA2_WRAPPER_WRAPPED_LIB)
        set(WRAPPED_LIB ${DEFINE_IA2_WRAPPER_WRAPPED_LIB})
    elseif(DEFINE_IA2_WRAPPER_WRAP_MAIN)
        set(WRAPPED_LIB ${TEST_NAME}-main)
    else()
        set(WRAPPED_LIB ${TEST_NAME}-original)
    endif()
    set(SRCS ${DEFINE_IA2_WRAPPER_SRCS})
    set(EXTRA_REWRITER_ARGS ${DEFINE_IA2_WRAPPER_EXTRA_REWRITER_ARGS})
    if(DEFINED DEFINE_IA2_WRAPPER_INCLUDE_DIR)
        set(INCLUDE_DIR ${DEFINE_IA2_WRAPPER_INCLUDE_DIR})
    else()
        # Use system path for system libs and `source/include` for in-tree libs
        if(${DEFINE_IA2_WRAPPER_USE_SYSTEM_HEADERS})
            pkg_check_modules(LIB REQUIRED lib${WRAPPED_LIB})
            set(INCLUDE_DIR ${LIB_INCLUDEDIR})
        else()
            set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
        endif()
    endif()

    if(DEFINED DEFINE_IA2_WRAPPER_CALLER_PKEY)
        set(CALLER_PKEY_OPTION "--compartment-pkey=${DEFINE_IA2_WRAPPER_CALLER_PKEY}")
    else()
        message(FATAL_ERROR
            "CALLER_PKEY (0-15) must be defined to build a wrapper")
    endif()

    if(DEFINED DEFINE_IA2_WRAPPER_TARGET_PKEY)
        set(TARGET_PKEY ${DEFINE_IA2_WRAPPER_TARGET_PKEY})
    else()
        set(TARGET_PKEY "0")
    endif()
    set(DEFINE_TARGET_PKEY "-DTARGET_PKEY=${TARGET_PKEY}")

    if(DEFINED DEFINE_IA2_WRAPPER_OUTPUT_DIR)
        set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${DEFINE_IA2_WRAPPER_OUTPUT_DIR})
    else()
        set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    set(ORIGINAL_SRCS ${SRCS})
    set(REWRITTEN_SRCS ${SRCS})
    list(TRANSFORM ORIGINAL_SRCS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
    list(TRANSFORM REWRITTEN_SRCS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)

    set(REWRITER_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/${WRAPPED_LIB}_shim/)

    set(WRAPPER_PREFIX ${WRAPPED_LIB}-shim)
    set(WRAPPER_SRC ${WRAPPER_PREFIX}.c)
    set(OUTPUT_HEADER ${CMAKE_CURRENT_BINARY_DIR}/${WRAPPER_PREFIX}.h)
    get_target_property(IA2_INCLUDE_DIR libia2 INCLUDE_DIRECTORIES)
    # Run the header rewriter
    add_custom_command(
        OUTPUT ${REWRITTEN_SRCS} ${WRAPPER_SRC} ${OUTPUT_HEADER}
        # Copy sources to their REWRITTEN_SRCS locations
        COMMAND cp -r ${ORIGINAL_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/
        COMMAND cp -r ${INCLUDE_DIR} ${REWRITER_INCLUDE_DIR}
        # Run the rewriter itself, mutating the sources
        COMMAND ia2-header-rewriter
          ${CALLER_PKEY_OPTION}
          --output-filename=${CMAKE_CURRENT_BINARY_DIR}/${WRAPPER_PREFIX}
          ${REWRITTEN_SRCS}
          --
          -I ${REWRITER_INCLUDE_DIR}
          -I ${IA2_INCLUDE_DIR}
          -isystem ${C_SYSTEM_INCLUDE}
          -isystem ${C_SYSTEM_INCLUDE_FIXED}
          -D_GNU_SOURCE
        DEPENDS ${ORIGINAL_SRCS} ia2-header-rewriter
        VERBATIM)

    # If we're producing a wrapper to call the main binary we don't need to link
    # against any library
    if(DEFINE_IA2_WRAPPER_WRAP_MAIN)
        unset(WRAPPED_LIB)
    endif()
    # Define wrapper library target
    add_library(${WRAPPER_TARGET} SHARED ${WRAPPER_SRC})
    if(LIBIA2_INSECURE)
        target_compile_definitions(${WRAPPER_TARGET} PUBLIC LIBIA2_INSECURE=1)
    endif()
    target_compile_definitions(${WRAPPER_TARGET} PRIVATE _GNU_SOURCE)
    target_compile_options(${WRAPPER_TARGET} PRIVATE
        "-Wno-deprecated-declarations"
        ${DEFINE_TARGET_PKEY}
        INTERFACE "-include" ${OUTPUT_HEADER})
    target_link_options(${WRAPPER_TARGET} PRIVATE "-Wl,-z,now")
    target_link_libraries(${WRAPPER_TARGET}
        INTERFACE -Wl,@${CMAKE_CURRENT_BINARY_DIR}/${WRAPPER_PREFIX}.args
        PUBLIC ${WRAPPED_LIB})

    # Add IA2 and wrapper include dirs
    target_include_directories(${WRAPPER_TARGET}
        INTERFACE ${REWRITER_INCLUDE_DIR}
    )

    target_link_libraries(${WRAPPER_TARGET} PRIVATE libia2)

    return()
endfunction()
