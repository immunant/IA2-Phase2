if (LIBIA2_AARCH64)
    set(UBSAN_FLAG "")
else()
    set(UBSAN_FLAG "-fsanitize=undefined")
endif()
# Creates a compartmentalized IA2 target
#
# add_ia2_compartment(<name> <EXECUTABLE|LIBRARY> PKEY <n> SOURCES <src>...
#                     [INCLUDE_DIRECTORIES <dir>...] [LIBRARIES <lib>...]
#                     [ENABLE_UBSAN])
#
# Creates either an IA2 executable or shared library called <name> with the given pkey <n>.
#
# <name> - Target name.
# PKEY (required) - The pkey for the compartment of this binary.
# SOURCES (required) - The set of source files.
# INCLUDE_DIRECTORIES (optional) - include directories used to build the executable. If
#       a relative path this will be interpreted as relative to the current source
#       directory.
# LIBRARIES (optional) - libraries to link against.
# ENABLE_UBSAN - If present, enable UBSAN for this executable.
function(add_ia2_compartment NAME TYPE)
  # Parse options
  set(options ENABLE_UBSAN)
  set(oneValueArgs PKEY)
  set(multiValueArgs LIBRARIES SOURCES INCLUDE_DIRECTORIES)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}"
    "${multiValueArgs}" ${ARGN})

  set(VALID_TYPES EXECUTABLE LIBRARY)
  if(NOT "${TYPE}" IN_LIST VALID_TYPES)
    message(FATAL_ERROR "${TYPE} is not a valid type, expected one of ${VALID_TYPES}")
  endif()

  if("${TYPE}" STREQUAL "EXECUTABLE")
    add_executable(${NAME})
  elseif("${ARG_PKEY}" GREATER "0")
    set(UNPADDED_LIB ${NAME}_unpadded)
    add_library(${UNPADDED_LIB} SHARED)
    pad_tls_library(${UNPADDED_LIB} ${NAME})
    set(NAME ${UNPADDED_LIB})
  else()
    add_library(${NAME} SHARED)
  endif()

  # The x86 version is missing a dependency here and libs rely on include path overlap to pick up
  # criterion header. I want to keep spoofed criterion static for simplicity so we add the -I
  # directly here.
  if (LIBIA2_AARCH64)
    target_include_directories(${NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/misc/spoofed_criterion/include)
  endif()
  target_compile_definitions(${NAME} PRIVATE
    IA2_ENABLE=1
    PKEY=${ARG_PKEY}
  )
  set_target_properties(${NAME} PROPERTIES PKEY ${ARG_PKEY})
  target_compile_options(${NAME} PRIVATE
    "-Werror=incompatible-pointer-types"
    "-fPIC"
  )

  set_property(TARGET ${NAME} PROPERTY ORIGINAL_SOURCES ${ARG_SOURCES})
  set_property(TARGET ${NAME} PROPERTY ORIGINAL_INCLUDE_DIRECTORIES ${ARG_INCLUDE_DIRECTORIES})

  if (ARG_ENABLE_UBSAN)
    # UBSAN requires passing this as both a compiler and linker flag
    target_compile_options(${NAME} PRIVATE ${UBSAN_FLAG})
    target_link_options(${NAME} PRIVATE ${UBSAN_FLAG})
  endif()

  target_link_libraries(${NAME} PRIVATE dl libia2 partition-alloc)
  target_link_options(${NAME} PRIVATE "-Wl,--export-dynamic")

  target_link_libraries(${NAME} PRIVATE ${ARG_LIBRARIES})

  create_compile_commands(${NAME} ${TYPE}
    PKEY ${ARG_PKEY}
    SOURCES ${ARG_SOURCES}
    INCLUDE_DIRECTORIES ${ARG_INCLUDE_DIRECTORIES}
  )

  if("${TYPE}" STREQUAL "EXECUTABLE")
    # Create and link call gates
    add_ia2_call_gates(${NAME} LIBRARIES ${ARG_LIBRARIES})
  endif()
endfunction()

# Prepend <basedir> to given paths, if relative.
#
# relative_to_absolute(<output_var> <basedir> <path>...)
function(relative_to_absolute OUTPUT BASEDIR)
  foreach(dir ${ARGN})
    if("${dir}" MATCHES "^/")
      # absolute path, so we want to leave it alone
      list(APPEND paths "${dir}")
    else()
      list(APPEND paths "${BASEDIR}/${dir}")
    endif()
  endforeach()
  set(${OUTPUT} ${paths} PARENT_SCOPE)
endfunction()

function(pad_tls_library INPUT OUTPUT)
  # Add command and target to generate the padded, sonamed library
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lib${OUTPUT}.so
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${INPUT}> ${CMAKE_CURRENT_BINARY_DIR}/lib${OUTPUT}.so
    COMMAND ${CMAKE_BINARY_DIR}/tools/pad-tls/pad-tls --allow-no-tls ${CMAKE_CURRENT_BINARY_DIR}/lib${OUTPUT}.so
    DEPENDS ${PAD_TLS_SRCS} tools pad-tls $<TARGET_FILE:${INPUT}>
    COMMENT "Padding TLS segment of wrapped library"
  )
  add_custom_target(${OUTPUT}-padding DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/lib${OUTPUT}.so")
  add_library(${OUTPUT} SHARED IMPORTED GLOBAL)
  set_property(TARGET ${OUTPUT} PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/lib${OUTPUT}.so")
  add_dependencies(${OUTPUT} ${OUTPUT}-padding)

  set_target_properties(${INPUT} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/unpadded"
    OUTPUT_NAME "${OUTPUT}"
  )
  target_link_options(${OUTPUT} INTERFACE $<TARGET_PROPERTY:${INPUT},INTERFACE_LINK_OPTIONS>)
endfunction()

# Create a fake target that builds the given sources
#
# We use this target to provide the input compile_commands.json for the source
# rewriter.
function(create_compile_commands NAME TYPE)
  # Parse options
  set(options "")
  set(oneValueArgs PKEY)
  set(multiValueArgs SOURCES INCLUDE_DIRECTORIES)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}"
    "${multiValueArgs}" ${ARGN})

  relative_to_absolute(SOURCES ${CMAKE_CURRENT_SOURCE_DIR} ${ARG_SOURCES})
  relative_to_absolute(INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR} ${ARG_INCLUDE_DIRECTORIES})

  # Create a temporary target to get a compile_commands.json. This target will
  # never be built.
  set(COMPILE_COMMAND_TARGET ${NAME}_compile_commands)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  if("${TYPE}" STREQUAL "EXECUTABLE")
    add_executable(${COMPILE_COMMAND_TARGET} EXCLUDE_FROM_ALL ${SOURCES})
  else()
    add_library(${COMPILE_COMMAND_TARGET} EXCLUDE_FROM_ALL SHARED ${SOURCES})
  endif()
  target_compile_definitions(${COMPILE_COMMAND_TARGET} PRIVATE
    IA2_ENABLE=0
    PKEY=${ARG_PKEY}
  )
  # Copy target properties from the real target. We might need to add more properties.
  target_link_libraries(${COMPILE_COMMAND_TARGET} PRIVATE $<TARGET_PROPERTY:${NAME},LINK_LIBRARIES>)
  target_include_directories(${COMPILE_COMMAND_TARGET} PRIVATE ${INCLUDE_DIRECTORIES})
  if (LIBIA2_AARCH64)
    target_include_directories(${COMPILE_COMMAND_TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/misc/spoofed_criterion/include)
  endif()
  set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
endfunction()

# We need to get the system headers specifically from clang
find_program(CLANG_EXE
  clang
  PATHS /usr/bin/clang
  DOC "Path to clang compiler")

if(NOT CLANG_EXE)
  message(FATAL_ERROR "Could not find Clang, please set CLANG_EXE manually")
endif()
message(STATUS "Found Clang executable: ${CLANG_EXE}")

# Get system header directories for header rewriter as it needs them explicitly passed
execute_process(COMMAND ${CLANG_EXE} -print-file-name=include
  OUTPUT_VARIABLE CLANG_HEADERS_INCLUDE
  OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "Found Clang headers: ${CLANG_HEADERS_INCLUDE}")

execute_process(COMMAND ${CLANG_EXE} -print-file-name=include-fixed
  OUTPUT_VARIABLE CLANG_HEADERS_INCLUDE_FIXED
  OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "Found Clang fixed headers: ${CLANG_HEADERS_INCLUDE_FIXED}")

file(GLOB REWRITER_SRCS ${CMAKE_SOURCE_DIR}/tools/rewriter/*.cpp)
file(GLOB PAD_TLS_SRCS ${CMAKE_SOURCE_DIR}/tools/pad-tls/*.c)
# This cannot be in the tools directory CMakeLists.txt because the target is for
# the top-level CMake project
add_custom_target(rewriter
    COMMAND ${CMAKE_COMMAND} --build . -t ia2-rewriter
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tools
    # tools dependency is for the CMake config step
    DEPENDS tools ${REWRITER_SRCS})

add_custom_target(pad-tls
    COMMAND ${CMAKE_COMMAND} --build . -t pad-tls
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tools
    DEPENDS tools ${PAD_TLS_SRCS})

# Create call gates for a target
#
# Creates call gates for executable target <name> and all of its
# compartmentalized dependencies. As part of this process we rewrite all given
# input sources for these targets and add the rewritten sources as the new
# sources for the targets (and similarly for include directories to pick up
# rewritten headers).
#
# This depends on our custom target properties PKEY, ORIGINAL_SOURCES, and
# ORIGINAL_INCLUDE_DIRECTORIES being set for any compartmentalized targets.
function(add_ia2_call_gates NAME)
  # Parse options
  set(options "")
  set(oneValueArgs "")
  set(multiValueArgs LIBRARIES EXTRA_REWRITER_ARGS)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}"
    "${multiValueArgs}" ${ARGN})

  get_target_property(PKEY ${NAME} PKEY)

  set(CALL_GATE_TARGET ${NAME}_call_gates)
  set(CALL_GATE_SRC ${CMAKE_CURRENT_BINARY_DIR}/${NAME}_call_gates.c)
  set(CALL_GATE_HDR ${CMAKE_CURRENT_BINARY_DIR}/${NAME}_call_gates.h)

  foreach(target ${NAME} ${ARG_LIBRARIES})
    set(target_pkey_set FALSE)
    if(TARGET ${target})
      get_property(target_pkey_set TARGET ${target} PROPERTY PKEY SET)
    endif()
    if(NOT ${target_pkey_set})
      if(TARGET ${target}_unpadded)
        get_property(target_pkey_set TARGET ${target}_unpadded PROPERTY PKEY SET)
      endif()
      if(${target_pkey_set})
        set(target ${target}_unpadded)
      endif()
    endif()

    set(REWRITER_OUTPUT_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/${NAME}_call_gates")

    if(${target_pkey_set})
      get_target_property(target_source_dir ${target} SOURCE_DIR)
      get_target_property(target_binary_dir ${target} BINARY_DIR)

      get_target_property(target_pkey ${target} PKEY)
      set(target_ld_args_file "${REWRITER_OUTPUT_PREFIX}_${target_pkey}.ld")
      target_link_options(${target} PRIVATE "-Wl,@${target_ld_args_file}")
      list(APPEND LD_ARGS_FILES "${target_ld_args_file}")

      set(target_objcopy_args_file "${REWRITER_OUTPUT_PREFIX}_${target_pkey}.objcopy")
      set(OBJCOPY_GLUE ${CMAKE_OBJCOPY} "--redefine-syms=${target_objcopy_args_file}")
      set(OBJCOPY_CMD ${OBJCOPY_GLUE} $<JOIN:$<TARGET_OBJECTS:${target}>, \\\; && ${OBJCOPY_GLUE} >)
      add_custom_command(TARGET ${target} PRE_LINK
                         COMMAND "${OBJCOPY_CMD}"
                         DEPENDS ${target_objcopy_args_file}
                         VERBATIM
                         COMMAND_EXPAND_LISTS)
      list(APPEND OBJCOPY_ARGS_FILES "${target_objcopy_args_file}")

      get_target_property(target_srcs ${target} ORIGINAL_SOURCES)
      relative_to_absolute(target_original_srcs ${target_source_dir} ${target_srcs})
      list(APPEND SOURCES ${target_original_srcs})
      relative_to_absolute(target_rewritten_srcs ${target_binary_dir} ${target_srcs})
      list(APPEND REWRITTEN_SOURCES ${target_rewritten_srcs})
      target_sources(${target} PRIVATE ${target_rewritten_srcs})

      get_target_property(target_include_dirs ${target} ORIGINAL_INCLUDE_DIRECTORIES)
      if(target_include_dirs)
        relative_to_absolute(target_original_include_dirs ${target_source_dir} ${target_include_dirs})
        list(APPEND INCLUDE_DIRECTORIES ${target_original_include_dirs})
        relative_to_absolute(target_rewritten_include_dirs ${target_binary_dir} ${target_include_dirs})
        list(APPEND REWRITTEN_INCLUDE_DIRECTORIES ${target_rewritten_include_dirs})
        if("${target_pkey}" GREATER "0")
          target_include_directories(${target} PRIVATE ${target_rewritten_include_dirs})
        else()
          target_include_directories(${target} PRIVATE ${target_original_include_dirs})
        endif()
      endif()

      # FIXME: This shouldn't be necessary but it seems aarch64-gcc < v13 might
      # default to --as-needed so this is needed to fix some runtime ld.so lookup
      # error
      if (LIBIA2_AARCH64)
          target_link_options(${target} PRIVATE "-Wl,--no-as-needed")
      endif()
      target_link_libraries(${target} PRIVATE ${CALL_GATE_TARGET})

      if("${target_pkey}" GREATER "0")
        # We set this privately on the executable so it doesn't get transitively
        # picked up by the compile command target.
        target_compile_options(${target} PRIVATE "-include${CALL_GATE_HDR}")
      endif()
    endif()
  endforeach()

  if (LIBIA2_AARCH64)
      set(ARCH_FLAG "--arch=aarch64")
  else()
      set(SYSROOT_FLAG
        --extra-arg=-isystem "--extra-arg=${CLANG_HEADERS_INCLUDE}"
        --extra-arg=-isystem "--extra-arg=${CLANG_HEADERS_INCLUDE_FIXED}")
  endif()
  add_custom_command(
    OUTPUT ${CALL_GATE_SRC} ${CALL_GATE_HDR}
           ${LD_ARGS_FILES} ${OBJCOPY_ARGS_FILES} ${REWRITTEN_SOURCES}
    COMMAND ${CMAKE_BINARY_DIR}/tools/rewriter/ia2-rewriter
        --output-prefix=${REWRITER_OUTPUT_PREFIX}
        --root-directory=${CMAKE_CURRENT_SOURCE_DIR}
        --output-directory=${CMAKE_CURRENT_BINARY_DIR}
        ${ARCH_FLAG}
        # Set the build path so the rewriter can find the compile_commands JSON
        -p=${CMAKE_BINARY_DIR}
        ${SYSROOT_FLAG}
        ${ARG_EXTRA_REWRITER_ARGS}
        ${SOURCES}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    # dependencies on custom targets (i.e. the rewriter) do not re-run this
    # command so we need to add a dependency on the rewriter's sources. We still
    # need the dependency on the custom rewriter target to make sure it gets
    # built the first time.
    DEPENDS ${SOURCES} ${REWRITER_SRCS} rewriter
    VERBATIM
  )

  add_custom_target(${NAME}-rewrite
    DEPENDS ${CALL_GATE_SRC} ${CALL_GATE_HDR}
            ${LD_ARGS_FILES} ${OBJCOPY_ARGS_FILES} ${REWRITTEN_SOURCES})

  add_library(${CALL_GATE_TARGET} SHARED ${CALL_GATE_SRC})
  add_dependencies(${CALL_GATE_TARGET} ${NAME}-rewrite)
  # This is needed to enable LIBIA2_DEBUG=1
  target_compile_definitions(${CALL_GATE_TARGET} PRIVATE IA2_ENABLE=1)
  if(LIBIA2_DEBUG)
    target_compile_definitions(${CALL_GATE_TARGET} PRIVATE LIBIA2_DEBUG=1 IA2_ENABLE=1)
  endif()
  target_compile_definitions(${CALL_GATE_TARGET} PRIVATE _GNU_SOURCE)
  target_link_libraries(${CALL_GATE_TARGET} PUBLIC libia2)
endfunction()
