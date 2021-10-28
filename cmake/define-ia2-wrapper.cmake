# Get system header directories for header rewriter as it needs them explicitly passed
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include
  OUTPUT_VARIABLE C_SYSTEM_INCLUDE
  OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include-fixed
  OUTPUT_VARIABLE C_SYSTEM_INCLUDE_FIXED
  OUTPUT_STRIP_TRAILING_WHITESPACE)

function(define_ia2_wrapper)
    # Parse options
    set(options USE_SYSTEM_HEADERS)
    set(oneValueArgs WRAPPER WRAPPED_LIB OUTPUT_HEADER INCLUDE_DIR)
    set(multiValueArgs HEADERS PRIVATE_HEADERS)
    cmake_parse_arguments(DEFINE_IA2_WRAPPER "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN} )

    # Shorter names for parsed args
    set(WRAPPER_TARGET ${DEFINE_IA2_WRAPPER_WRAPPER})
    set(WRAPPED_LIB ${DEFINE_IA2_WRAPPER_WRAPPED_LIB})
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
          ${CMAKE_CURRENT_BINARY_DIR}/${WRAPPER_SRC}
          ${REWRITTEN_HEADERS}
          --
          -I ${REWRITTEN_HEADER_DIR}
          -isystem ${C_SYSTEM_INCLUDE}
          -isystem ${C_SYSTEM_INCLUDE_FIXED}
        DEPENDS ${HEADER_SRCS}
        VERBATIM)

    # Define wrapper library target
    add_library(${WRAPPER_TARGET} SHARED ${WRAPPER_SRC})
    target_compile_options(${WRAPPER_TARGET} PRIVATE "-Wno-deprecated-declarations")
    target_link_libraries(${WRAPPER_TARGET}
        PRIVATE -Wl,--version-script,${CMAKE_CURRENT_BINARY_DIR}/${WRAPPER_SRC}.syms
        PUBLIC ${WRAPPED_LIB})

    # Add IA2 and wrapper include dirs
    target_include_directories(${WRAPPER_TARGET}
        BEFORE PUBLIC ${IA2_INCLUDE_DIR}
        INTERFACE ${REWRITTEN_HEADER_DIR}
    )

    return()
endfunction()
