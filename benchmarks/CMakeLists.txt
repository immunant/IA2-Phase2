include("../cmake/define-test.cmake")

add_library(b63 INTERFACE)
target_include_directories(b63 INTERFACE ${PROJECT_SOURCE_DIR}/external/b63/include)

add_subdirectory(microbench)
