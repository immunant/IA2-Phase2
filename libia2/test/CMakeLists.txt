add_executable(permissive_mode_test permissive_mode.c)
target_link_libraries(permissive_mode_test libia2)

if(NOT LIBIA2_INSECURE)
  add_test(NAME permissive_mode_test COMMAND permissive_mode_test)
endif()
