# No-op for compatibility
function(define_ia2_wrapper)
    # Parse options
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs ORIGINAL_TARGETS EXTRA_REWRITER_ARGS)
    cmake_parse_arguments(DEFINE_IA2_WRAPPER "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN} )

endfunction()
