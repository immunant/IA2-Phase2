add_library(simple1 SHARED libsimple1/simple1.c)
target_link_options(simple1 PRIVATE "-Wl,-z,now")

define_ia2_wrapper(
    WRAPPER simple1-wrapper
    WRAPPED_LIB simple1
    OUTPUT_HEADER simple1_ia2.h
    HEADERS simple1.h
    PRIVATE_HEADERS hooks.h
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libsimple1
)

add_executable(simple1-main main.c)
target_link_libraries(simple1-main PRIVATE simple1-wrapper ${IA2_LIB})
target_compile_options(simple1-main PRIVATE -Werror=incompatible-pointer-types)

add_dependencies(check-ia2 simple1-main)
