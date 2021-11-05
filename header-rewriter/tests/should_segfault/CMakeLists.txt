# Use the directory as the test name
get_filename_component(TEST_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(ORIGINAL_LIB ${TEST_NAME}-original)
set(WRAPPER ${TEST_NAME}-wrapper)
set(MAIN ${TEST_NAME}-main)

if(LIBIA2_INSECURE)
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/should_segfault.insecure.out)
else()
    set(STDOUT_REF ${CMAKE_CURRENT_SOURCE_DIR}/Output/should_segfault.out)
endif()
    configure_file(${STDOUT_REF} ${CMAKE_CURRENT_BINARY_DIR}/should_segfault.out)

set(HEADERS print_secret.h)

# Build the wrapper lib
define_ia2_wrapper(
    WRAPPER ${WRAPPER}
    WRAPPED_LIB ${ORIGINAL_LIB}
    HEADERS ${HEADERS}
)

# Build the wrapped lib
add_library(${ORIGINAL_LIB} SHARED print_secret.c)
set(ORIGINAL_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/)
target_include_directories(${ORIGINAL_LIB} BEFORE PRIVATE ${ORIGINAL_HEADER_DIR})
target_link_options(${ORIGINAL_LIB} PRIVATE "-Wl,-z,now")

# Build the test
add_executable(${MAIN} main.c)
target_link_directories(main PRIVATE ${IA2_LIB_DIR})
target_link_libraries(${MAIN} PRIVATE
    ${WRAPPER}
    ${IA2_LIB})

add_dependencies(check-ia2 ${MAIN})
