set(IA2_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/../include)

add_library(simple1 SHARED libsimple1/simple1.c)

add_custom_target(
  simple1-ia2
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1/simple1.h
    ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ia2-header-rewriter
    --output-header simple1_ia2.h
    ${CMAKE_CURRENT_BINARY_DIR}/wrapper.c
    simple1.h
    --
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1/simple1.h
  BYPRODUCTS simple1_ia2.h simple1.h wrapper.c
  VERBATIM
  )

add_library(simple1-wrapper SHARED wrapper.c)
target_link_libraries(simple1-wrapper PRIVATE simple1)
target_link_options(simple1-wrapper PRIVATE
  -Wl,--version-script,${CMAKE_CURRENT_BINARY_DIR}/wrapper.c.syms)
target_include_directories(simple1-wrapper BEFORE PRIVATE
  ${IA2_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1
  )

add_executable(simple1-main main.c)
target_link_libraries(simple1-main PRIVATE simple1-wrapper)
add_dependencies(simple1-main simple1-ia2)
target_compile_options(simple1-main PRIVATE -Werror=incompatible-pointer-types)
target_include_directories(simple1-main BEFORE PRIVATE
  ${IA2_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1
  )

add_dependencies(check-ia2 simple1-main)
