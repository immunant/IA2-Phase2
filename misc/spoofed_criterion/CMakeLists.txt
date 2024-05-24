add_library(criterion STATIC test_runner.c)
target_include_directories(criterion
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
