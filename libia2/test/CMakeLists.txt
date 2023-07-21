add_executable(permissive_mode_test permissive_mode.c)
target_link_libraries(permissive_mode_test libia2)
target_link_options(permissive_mode_test PRIVATE "-Wl,-z,now")

if(NOT LIBIA2_INSECURE)
  add_test(NAME permissive_mode_test COMMAND permissive_mode_test)
endif()
