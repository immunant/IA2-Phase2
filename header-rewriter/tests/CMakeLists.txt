enable_testing()

include("../../cmake/define-ia2-wrapper.cmake")
include("../../cmake/define-test.cmake")

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
  MAIN_CONFIG
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
  )

set(IA2_TEST_DEPENDS ia2-header-rewriter)

add_lit_testsuite(check-ia2 "Running the IA2 tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${IA2_TEST_DEPENDS} partition-alloc
  )
set_target_properties(check-ia2 PROPERTIES FOLDER "tests")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set up 'check' alias
add_custom_target(check)
add_dependencies(check check-ia2)
set_target_properties(check PROPERTIES FOLDER "tests")

add_subdirectory(ffmpeg)
add_subdirectory(function_allowlist)
add_subdirectory(header_includes)
add_subdirectory(heap_two_keys)
add_subdirectory(libusb)
add_subdirectory(macro_attr)
add_subdirectory(minimal)
#add_subdirectory(namespaces)
add_subdirectory(ro_sharing)
# We don't actually compile the `omit_wrappers` test, but it is picked up by LIT.
add_subdirectory(shared_data)
add_subdirectory(should_segfault)
add_subdirectory(simple1)
add_subdirectory(structs)
add_subdirectory(trusted_indirect)
add_subdirectory(trusted_direct)
add_subdirectory(untrusted_indirect)
add_subdirectory(two_keys_minimal)
add_subdirectory(two_shared_ranges)
add_subdirectory(global_fn_ptr)
add_subdirectory(read_config)
