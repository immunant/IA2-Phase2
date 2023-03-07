# Define a copy of a library that uses thread-local storage which pads the TLS
# segment as required by IA2.
#
# This creates a new library whose name adds "-wrapped" to the name of the
# specified library, which may be a system library or one built by CMake.
# Other targets should then depend normally on the "-wrapped" target.
#
# LIB - Name of the library to copy (e.g. avcodec)
# OUTPUT_DIR - Where the copy of the library should be placed.
# SYSTEM - Provide if this is a system library, to locate the library itself.
function(add_tls_padded_library)
    # Parse options
    set(options SYSTEM)
    set(oneValueArgs LIB OUTPUT_DIR)
    set(multiValueArgs )
    cmake_parse_arguments(ADD_TLS_PADDED_LIBRARY "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN} )

    # Abbreviate options
    if(DEFINED ADD_TLS_PADDED_LIBRARY_LIB)
        set(LIB ${ADD_TLS_PADDED_LIBRARY_LIB})
        set(LIB_PADDED "${LIB}_padded")
    else()
        message(FATAL_ERROR "add-tls-padded-library requires a LIB argument")
    endif()

    if(DEFINED ADD_TLS_PADDED_LIBRARY_OUTPUT_DIR)
        set(OUTPUT_DIR ${ADD_TLS_PADDED_LIBRARY_OUTPUT_DIR})
    else()
        message(FATAL_ERROR "add-tls-padded-library requires an OUTPUT_DIR argument")
    endif()

    # Use system path for system libs and current binary dir for in-tree libs
    if(${ADD_TLS_PADDED_LIBRARY_SYSTEM})
        pkg_check_modules(LIB REQUIRED lib${WRAPPED_LIB})
        set(LIB_DIR ${LIB_LIBDIR})
    else()
        set(LIB_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    # Add command and target to generate the padded, sonamed library
    add_custom_command(
        OUTPUT ${OUTPUT_DIR}/lib${LIB_PADDED}.so
        COMMAND ${CMAKE_COMMAND} -E copy ${LIB_DIR}/lib${LIB}.so ${OUTPUT_DIR}/lib${LIB_PADDED}.so
        COMMAND pad-tls --allow-no-tls ${OUTPUT_DIR}/lib${LIB_PADDED}.so
        COMMAND patchelf --set-soname lib${LIB_PADDED}.so ${OUTPUT_DIR}/lib${LIB_PADDED}.so
        DEPENDS pad-tls ${LIB_DIR}/lib${LIB}.so
        COMMENT "Padding TLS segment of wrapped library"
    )
    add_custom_target(${LIB_PADDED}-tgt DEPENDS "${OUTPUT_DIR}/lib${LIB_PADDED}.so")
    add_library(${LIB_PADDED} SHARED IMPORTED)
    set_property(TARGET ${LIB_PADDED} PROPERTY IMPORTED_LOCATION "${OUTPUT_DIR}/lib${LIB_PADDED}.so")
    add_dependencies(${LIB_PADDED} ${LIB_PADDED}-tgt)
endfunction()
