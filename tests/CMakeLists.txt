include(CTest)

enable_testing()

include("../cmake/define-ia2-wrapper.cmake")
include("../cmake/define-test.cmake")

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM: ${LLVM_DIR} (found version \"${LLVM_PACKAGE_VERSION}\")")
find_package(Clang REQUIRED CONFIG)
message(STATUS "Found Clang: ${Clang_DIR}")

# We use the lit CMake functions from LLVM
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
  MAIN_CONFIG
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
  )

set(IA2_TEST_DEPENDS tools)

add_lit_testsuite(check-ia2 "Running the IA2 tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${IA2_TEST_DEPENDS} partition-alloc
  ARGS "--verbose"
  )
set_target_properties(check-ia2 PROPERTIES FOLDER "tests")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set up 'check' target and check-exec on arm
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)
if (NOT LIBIA2_AARCH64)
    add_dependencies(check check-ia2)
endif()
set_target_properties(check PROPERTIES FOLDER "tests")

add_subdirectory(abi)
# add_subdirectory(ffmpeg)
add_subdirectory(header_includes)
# add_subdirectory(libusb)
add_subdirectory(macro_attr)
add_subdirectory(minimal)
# FIXME: This test is only used for aarch64 since we need to build criterion
# from source to support AArch64
add_subdirectory(minimal_no_criterion)
# TODO: support C++ namespaces
#add_subdirectory(namespaces)
add_subdirectory(recursion)
# We don't actually compile the `omit_wrappers` test, but it is picked up by LIT.
add_subdirectory(shared_data)
add_subdirectory(global_fn_ptr)
add_subdirectory(rewrite_macros)
add_subdirectory(sighandler)

# The following tests are not supported on ARM64 yet
if (NOT LIBIA2_AARCH64)
    # Expected to have compartment violations, but we aren't enforcing yet:
    add_subdirectory(destructors)
    add_subdirectory(ro_sharing)
    add_subdirectory(should_segfault)
    add_subdirectory(trusted_direct)
    add_subdirectory(untrusted_indirect)
    add_subdirectory(two_keys_minimal)
    add_subdirectory(two_shared_ranges)
    # TODO(#413): Fix these tests
    add_subdirectory(heap_two_keys)
    add_subdirectory(three_keys_minimal)

    # strange bug with indirect calls
    add_subdirectory(read_config)
    # ARM does not support threads yet
    add_subdirectory(threads)
    add_subdirectory(protected_threads)
    # test calls PKRU-specific functions for logging
    add_subdirectory(tls_protected)
    # no permissive mode on ARM
    add_subdirectory(permissive_mode)
    # rewriter issue on ARM, likely caused by include dirs and spoofed criterion headers
    add_subdirectory(structs)
    # not sure why this is failing
    add_subdirectory(mmap_loop)
    # we don't support call gates for indirect transitions yet
    add_subdirectory(simple1)
    add_subdirectory(trusted_indirect)
    add_subdirectory(rewrite_fn_ptr_eq)
endif()
