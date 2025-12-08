set(LIBIA2_AARCH64 TRUE)

# The libc/loader compartment feature is x86_64-only.  Force the option off
# for all AArch64 builds to avoid pulling in incomplete loader PKRU paths.
set(IA2_LIBC_COMPARTMENT OFF CACHE BOOL "Enable libc/ld.so compartmentalization and exit callgate support" FORCE)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# we cannot use relative paths here, but we should use our forked clang to build all aarch64 binaries to ensure instrumentation of loads/stores
#set(CMAKE_C_COMPILER ../../llvm-project/build/bin/clang-19)
#set(CMAKE_CXX_COMPILER ../../llvm-project/build/bin/clang-19)

set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)

add_compile_options("-march=armv8+memtag")

set(CMAKE_OBJCOPY llvm-objcopy)

set(CMAKE_C_HOST_COMPILER gcc)
set(CMAKE_CXX_HOST_COMPILER g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
