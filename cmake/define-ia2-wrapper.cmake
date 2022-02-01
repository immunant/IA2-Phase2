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
# HEADERS - Public headers to rewrite
# PRIVATE_HEADERS - Private headers to rewrite
# USE_SYSTEM_HEADERS - Use headers for library installed on the system
# WRAP_MAIN - Creates a shim for the main binary and changes the default target
#             name.
# WRAPPER - Wrapper target name. Defaults to ${TEST_NAME}-wrapper or
#           ${TEST_NAME}-main-wrapper.
# WRAPPED_LIB - Target name for library to wrap.Defaults to ${TEST_NAME}-original or
#               ${TEST_NAME}-main.
# OUTPUT_HEADER - Header for IA2's type-specific function pointer macros.
#                 Defaults to ${WRAPPED_LIB}_fn_ptr_ia2.h.
# INCLUDE_DIR - Added to search path in rewriter invocation. Defaults to
#               SRC_DIR/include.
# COMPARTMENT_PKEy - Key to use in compartment transitions.
function(define_ia2_wrapper)
    # Parse options
    set(options USE_SYSTEM_HEADERS WRAP_MAIN)
    set(oneValueArgs WRAPPER WRAPPED_LIB OUTPUT_HEADER INCLUDE_DIR COMPARTMENT_KEY)
    set(multiValueArgs HEADERS PRIVATE_HEADERS)
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
    set(HEADERS ${DEFINE_IA2_WRAPPER_HEADERS})
    set(PRIVATE_HEADERS ${DEFINE_IA2_WRAPPER_PRIVATE_HEADERS})
    if(DEFINED DEFINE_IA2_WRAPPER_OUTPUT_HEADER)
        set(OUTPUT_HEADER ${DEFINE_IA2_WRAPPER_OUTPUT_HEADER})
    else()
        set(OUTPUT_HEADER "${WRAPPED_LIB}_fn_ptr_ia2.h")
    endif()
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
    if(DEFINED DEFINE_IA2_WRAPPER_COMPARTMENT_KEY)
        set(COMPARTMENT_PKEY_OPTION
            "--compartment-key=${DEFINE_IA2_WRAPPER_COMPARTMENT_KEY}")
    endif()

    # Collect headers
    set(ORIGINAL_HEADER_DIR ${INCLUDE_DIR})
    set(REWRITTEN_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR})

    if(${DEFINE_IA2_WRAPPER_USE_SYSTEM_HEADERS})
        # Grab system headers
        set(COPIED_HEADERS)

        foreach(SYSTEM_HEADER ${HEADERS})
            # Make path absolute
            if(NOT IS_ABSOLUTE ${SYSTEM_HEADER})
                set(SYSTEM_HEADER ${INCLUDE_DIR}/${SYSTEM_HEADER})
            endif()

            # Copy the header to build dir under `system` subdir
            get_filename_component(HEADER_NAME ${SYSTEM_HEADER} NAME)
            set(COPIED_HEADER ${REWRITTEN_HEADER_DIR}/system/${HEADER_NAME})
            add_custom_command(
                OUTPUT ${COPIED_HEADER}
                COMMAND mkdir -p ${REWRITTEN_HEADER_DIR}
                COMMAND cp ${SYSTEM_HEADER} ${COPIED_HEADER}
                DEPENDS ${SYSTEM_HEADER}
            )

            # Build list absolute paths of input headers
            list(APPEND COPIED_HEADERS ${COPIED_HEADER})
            # Build list of absolute paths of mutated outputs from rewriter
            list(APPEND REWRITTEN_HEADERS ${REWRITTEN_HEADER_DIR}/${HEADER_NAME})
        endforeach()

        set(HEADER_SRCS ${COPIED_HEADERS} ${PRIVATE_HEADERS})

        # Recursively copy just these headers to run the rewriter on
        set(HEADER_SRC_DIRS ${COPIED_HEADERS} ${PRIVATE_HEADERS})
    else()
        # Build absolute paths of mutated outputs from rewriter
        set(REWRITTEN_HEADERS ${HEADERS})
        list(TRANSFORM REWRITTEN_HEADERS PREPEND ${REWRITTEN_HEADER_DIR}/)

        # Build absolute paths of header inputs
        set(HEADER_SRCS ${HEADERS} ${PRIVATE_HEADERS})
        list(TRANSFORM HEADER_SRCS PREPEND ${ORIGINAL_HEADER_DIR}/)

        # Glob dirs to copy to generate not-yet-mutated outputs
        file(GLOB HEADER_SRC_DIRS ${ORIGINAL_HEADER_DIR}/*)
    endif()

    set(WRAPPER_SRC ${WRAPPED_LIB}_wrapper.c)

    # Run the header rewriter
    add_custom_command(
        OUTPUT ${REWRITTEN_HEADERS} ${WRAPPER_SRC}
        # Copy headers to their REWRITTEN_HEADERS locations
        COMMAND cp -r ${HEADER_SRC_DIRS} ${REWRITTEN_HEADER_DIR}
        # Run the rewriter itself, mutating the headers
        COMMAND ia2-header-rewriter
          --output-header ${REWRITTEN_HEADER_DIR}/${OUTPUT_HEADER}
          ${COMPARTMENT_PKEY_OPTION}
          ${CMAKE_CURRENT_BINARY_DIR}/${WRAPPER_SRC}
          ${REWRITTEN_HEADERS}
          --
          -fgnuc-version=6
          -I ${REWRITTEN_HEADER_DIR}
          -isystem ${C_SYSTEM_INCLUDE}
          -isystem ${C_SYSTEM_INCLUDE_FIXED}
        DEPENDS ${HEADER_SRCS}
        VERBATIM)

    # If we're producing a wrapper for the main binary we don't need to link
    # against any library
    if(DEFINE_IA2_WRAPPER_WRAP_MAIN)
        unset(WRAPPED_LIB)
    endif()
    # Define wrapper library target
    add_library(${WRAPPER_TARGET} SHARED ${WRAPPER_SRC})
    target_compile_options(${WRAPPER_TARGET} PRIVATE "-Wno-deprecated-declarations")
    target_link_options(${WRAPPER_TARGET} PRIVATE "-Wl,-z,now")
    target_link_libraries(${WRAPPER_TARGET}
        PRIVATE -Wl,--version-script,${CMAKE_CURRENT_BINARY_DIR}/${WRAPPER_SRC}.syms
        PUBLIC ${WRAPPED_LIB}
        INTERFACE ${IA2_LIB})

    # Add IA2 and wrapper include dirs
    target_include_directories(${WRAPPER_TARGET}
        BEFORE PUBLIC ${IA2_INCLUDE_DIR}
        INTERFACE ${REWRITTEN_HEADER_DIR}
    )

    add_dependencies(${WRAPPER_TARGET} libia2)

    return()
endfunction()
