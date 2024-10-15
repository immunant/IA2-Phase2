add_library(ia2_test_runner STATIC test_runner.c)
target_include_directories(ia2_test_runner
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
