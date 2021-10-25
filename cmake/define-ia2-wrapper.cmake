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
        set(OUTPUT_HEADER "fn_ptr_ia2.h")
    endif()
    if(DEFINED DEFINE_IA2_WRAPPER_INCLUDE_DIR)
        set(INCLUDE_DIR ${DEFINE_IA2_WRAPPER_INCLUDE_DIR})
    else()
        set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/)
    endif()

    # Collect headers
    set(ORIGINAL_HEADER_DIR ${INCLUDE_DIR})
    set(REWRITTEN_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR})

    if(${DEFINE_IA2_WRAPPER_USE_SYSTEM_HEADERS})
        # Grab system headers
        set(COPIED_HEADERS)
        foreach(SYSTEM_HEADER ${HEADERS})
            get_filename_component(HEADER_NAME ${SYSTEM_HEADER} NAME)
            set(COPIED_HEADER ${REWRITTEN_HEADER_DIR}/system/${HEADER_NAME})
            add_custom_command(
                OUTPUT ${COPIED_HEADER}
                COMMAND mkdir -p ${REWRITTEN_HEADER_DIR}
                COMMAND cp ${SYSTEM_HEADER} ${COPIED_HEADER}
                DEPENDS ${SYSTEM_HEADER}
            )
            # Build list of input headers
            list(APPEND COPIED_HEADERS ${COPIED_HEADER})
            # Build list of mutated outputs from rewriter
            list(APPEND REWRITTEN_HEADERS ${REWRITTEN_HEADER_DIR}/${HEADER_NAME})
        endforeach()

        set(HEADER_SRCS ${COPIED_HEADERS} ${PRIVATE_HEADERS})
    else()
        # Build list of mutated outputs from rewriter
        set(REWRITTEN_HEADERS ${HEADERS})
        list(TRANSFORM REWRITTEN_HEADERS PREPEND ${REWRITTEN_HEADER_DIR}/)

        # Define list of input headers
        set(COPIED_HEADERS ${HEADERS})

        set(HEADER_SRCS ${COPIED_HEADERS} ${PRIVATE_HEADERS})
        list(TRANSFORM HEADER_SRCS PREPEND ${ORIGINAL_HEADER_DIR}/)
    endif()


    # Run the header rewriter itself
    add_custom_command(
        OUTPUT ${REWRITTEN_HEADERS} wrapper.c
        # Copy headers to their REWRITTEN_HEADERS locations
        COMMAND cp ${HEADER_SRCS} ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ia2-header-rewriter
          --output-header ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_HEADER}
          ${CMAKE_CURRENT_BINARY_DIR}/wrapper.c
          ${REWRITTEN_HEADERS}
          --
          -I ${REWRITTEN_HEADER_DIR}
          -isystem ${C_SYSTEM_INCLUDE}
          -isystem ${C_SYSTEM_INCLUDE_FIXED}
        DEPENDS ${HEADER_SRCS}
        VERBATIM)

    # Define wrapper library target
    add_library(${WRAPPER_TARGET} SHARED wrapper.c)
    target_compile_options(${WRAPPER_TARGET} PRIVATE "-Wno-deprecated-declarations")
    target_link_libraries(${WRAPPER_TARGET}
        PRIVATE -Wl,--version-script,${CMAKE_CURRENT_BINARY_DIR}/wrapper.c.syms
        PUBLIC ${WRAPPED_LIB})

    # Add IA2 and wrapper include dirs
    target_include_directories(${WRAPPER_TARGET}
        BEFORE PUBLIC ${IA2_INCLUDE_DIR}
        INTERFACE ${REWRITTEN_HEADER_DIR}
    )

    return()
endfunction()
