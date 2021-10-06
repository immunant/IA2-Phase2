enable_testing()

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
  MAIN_CONFIG
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
  )

set(IA2_TEST_DEPENDS ia2-header-rewriter)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  set(C_SYSTEM_INCLUDE /usr/lib/gcc/x86_64-pc-linux-gnu/${CMAKE_C_COMPILER_VERSION}/include/)
elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
  set(C_SYSTEM_INCLUDE /usr/lib/clang/${CMAKE_C_COMPILER_VERSION}/include/)
else ()
  message(FATAL_ERROR "Using unknown compiler. Unable to get system include directory")
endif()

add_lit_testsuite(check-ia2 "Running the IA2 tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${IA2_TEST_DEPENDS}
  )
set_target_properties(check-ia2 PROPERTIES FOLDER "tests")

# Set up 'check' alias
add_custom_target(check)
add_dependencies(check check-ia2)
set_target_properties(check PROPERTIES FOLDER "tests")

add_subdirectory(header_includes)
add_subdirectory(macro_attr)
add_subdirectory(minimal)
add_subdirectory(simple1)
add_subdirectory(libusb)
#add_subdirectory(namespaces)
