add_library(ia2_test_runner STATIC test_runner.c)
target_include_directories(ia2_test_runner
    PRIVATE ${CMAKE_SOURCE_DIR}/runtime/libia2/include)
