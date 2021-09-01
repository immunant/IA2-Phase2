# These are hard_coded in configure_lit_site_cfg

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
  MAIN_CONFIG
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
  )

set(IA2_TEST_DEPENDS ia2-header-rewriter)

add_lit_testsuite(check-ia2 "Running the IA2 tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${IA2_TEST_DEPENDS}
  )
set_target_properties(check-ia2 PROPERTIES FOLDER "Tests")

add_lit_testsuites(IA2 ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${IA2_TEST_DEPENDS}
  )

# Setup an alias for 'check-all'.
add_custom_target(check)
add_dependencies(check check-all)
set_target_properties(check PROPERTIES FOLDER "Tests")
