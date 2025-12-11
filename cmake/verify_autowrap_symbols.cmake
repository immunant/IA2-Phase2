# verify_autowrap_symbols.cmake
#
# Static verification that the loader callgate autowrap feature generated
# wrappers for All expected loader functions in the call-gate DSO.
#
# Handles both:
#   - Single-caller case: bare __wrap_dlopen
#   - Multicaller case: __wrap_dlopen_from_2, __wrap_dlopen_from_3, etc.
#
# Usage: cmake -DCALLGATES_SO=/path/to/libfoo_call_gates.so -P verify_autowrap_symbols.cmake

if(NOT DEFINED CALLGATES_SO)
    message(FATAL_ERROR "CALLGATES_SO not defined")
endif()

if(NOT EXISTS "${CALLGATES_SO}")
    message(FATAL_ERROR "Call-gate DSO not found: ${CALLGATES_SO}")
endif()

# All loader functions that autowrap should generate wrappers for.
# Each may appear as bare __wrap_<fn> (single-caller) or __wrap_<fn>_from_<N> (multicaller).
set(EXPECTED_FUNCTIONS
    _dl_debug_state
    dl_iterate_phdr
    dladdr
    dladdr1
    dlclose
    dlerror
    dlinfo
    dlmopen
    dlopen
    dlsym
    dlvsym
)

# Locate nm explicitly to avoid PATH issues on minimal or cross environments.
find_program(IA2_NM_EXECUTABLE nm)
if(NOT IA2_NM_EXECUTABLE)
    message(FATAL_ERROR "nm not found in PATH; set IA2_NM_EXECUTABLE to your nm")
endif()

# Run nm -D to get dynamic symbols
execute_process(
    COMMAND ${IA2_NM_EXECUTABLE} -D "${CALLGATES_SO}"
    OUTPUT_VARIABLE NM_OUTPUT
    ERROR_VARIABLE NM_ERROR
    RESULT_VARIABLE NM_RESULT
)

if(NOT NM_RESULT EQUAL 0)
    message(FATAL_ERROR "${IA2_NM_EXECUTABLE} -D failed on ${CALLGATES_SO}: ${NM_ERROR}")
endif()

# Check each expected function has at least one wrapper form:
#   - Bare: __wrap_<fn>
#   - Suffixed: __wrap_<fn>_from_<N> where N is a pkey number
set(MISSING_FUNCTIONS "")
set(FOUND_COUNT 0)
foreach(FN ${EXPECTED_FUNCTIONS})
    set(BARE_SYM "__wrap_${FN}")
    # Match bare symbol (must not be followed by _from to avoid false positives)
    # Pattern: address, space, T/W (strong/weak), space, symbol name, end-of-line or space
    string(REGEX MATCH "[0-9A-Fa-f]+ [TWtw] ${BARE_SYM}(\n| )" BARE_MATCH "${NM_OUTPUT}")
    # Match suffixed symbol: __wrap_<fn>_from_<digit>
    string(REGEX MATCH "[0-9A-Fa-f]+ [TWtw] ${BARE_SYM}_from_[0-9]+" SUFFIXED_MATCH "${NM_OUTPUT}")

    if(BARE_MATCH OR SUFFIXED_MATCH)
        math(EXPR FOUND_COUNT "${FOUND_COUNT} + 1")
    else()
        list(APPEND MISSING_FUNCTIONS "${FN}")
    endif()
endforeach()

list(LENGTH EXPECTED_FUNCTIONS EXPECTED_COUNT)

if(MISSING_FUNCTIONS)
    message(FATAL_ERROR
        "Loader autowrap verification FAILED\n"
        "  Found wrappers for: ${FOUND_COUNT}/${EXPECTED_COUNT} functions\n"
        "  Missing: ${MISSING_FUNCTIONS}\n"
        "  DSO: ${CALLGATES_SO}\n"
        "  (Checked for both __wrap_<fn> and __wrap_<fn>_from_<N> patterns)")
endif()

message(STATUS "Loader autowrap OK - all ${EXPECTED_COUNT} functions have wrappers in ${CALLGATES_SO}")
