# Building IA2 and compartmentalized programs for AArch64

Because the AArch64 port of IA2 requires changes to compiler code generation
(reserving the x18 register and instrumenting memory accesses),
all code executing in a compartmentalized process must be built with a modified toolchain.

To do so, the first step is to build the modified toolchain itself.
The toolchain is available at <https://github.com/immunant/llvm-project/tree/fw/instrument-llvm-ir>,
and can be built by running the [`build.sh`](https://github.com/immunant/llvm-project/blob/fw/instrument-llvm-ir/build.sh) script provided.
This will build LLVM, the Clang compiler, and then use the built Clang compiler to build the LLVM Builtins library.

## Using the modified toolchain

Because the toolchain instruments all memory accesses by applying the tag stored in the x18 register,
the `-ffixed-x18` flag must always be passed when compiling code with it.

This also serves to ensure that the modified toolchain is not being used mistakenly, e.g. for host code.

## Building runtime libraries

Beneath the C standard library itself, the compiler toolchain implements various functionality
(such as process setup and pre-termination callbacks) using "runtime libraries".
These libraries are built once and distributed with the compiler to link into
each program that the compiler builds.
To avoid code not compiled by the modified toolchain being linked into compartmentalized binaries,
a separate copy of these runtime libraries must be built by the modified toolchain.
This is achieved using the [`cross-build-rtlibs.sh`](https://github.com/immunant/llvm-project/blob/fw/instrument-llvm-ir/cross-build-rtlibs.sh) script in the modified LLVM branch.

## Building IA2 using the toolchain and libraries

Once the modified toolchain and runtime libraries have been built, they must be used to build IA2.

For later convenience, we define `toolchain_dir` to point to our compiler toolchain and custom build of runtime libraries.
Specifically, `toolchain_dir` should be a directory containing our Clang binary at the relative path `bin/clang-19`.
For example, if we cloned `llvm-project` adjacent to IA2:
```
toolchain_dir=$(pwd)/../llvm-project/build
```

If the host is an x86_64 system, this consitutes a cross-compiling build,
so appropriate flags must be passed to specify the target architecture variant and toolchain paths:
```
cross_flags="-isystem /usr/aarch64-linux-gnu/include --gcc-toolchain=/usr -march=armv8.5-a+memtag"
```

We must pass the `-ffixed-x18` argument when using the modified toolchain:
```
cross_flags+=" -ffixed-x18"
```

And the compiler must be directed to use the LLVM implementation of the C runtime libraries and C++ standard library,
which must be accompanied by its headers:
```
cross_flags+=" --rtlib=compiler-rt --stdlib=libc++ -I${rtlibs_dir}/include/c++/v1"
```

We also define an `rtlibs_dir` variable to point to the path at which we compiled the C runtime libraries.
This directory should contain `compiler-rt/lib/linux/crtbeginS.o`.
Given the runtime libraries built at the path `${rtlibs_dir}`,
the link stage of compilation must be directed to search for the runtime libraries themselves
with the `-B` flag (for startup files) and `-L` flag (for the C++ standard library):

```
cross_link_flags="-B${rtlibs_dir}/compiler-rt/lib/linux -L${rtlibs_dir}/lib"
```

In addition, the Clang toolchain must be directed to link its own runtime libraries rather than system-provided ones:
```
cross_link_flags+=" --rtlib=compiler-rt --unwindlib=libunwind"
```

The linker must be directed to sysroot for other libraries to be used in the build:
```
export LDFLAGS="-L/usr/aarch64-linux-gnu/lib"
```

Finally, we can use these flags for compilation with CMake, adding these flags to our `cmake` invocation:

```
cmake .. \
-DCMAKE_CROSSCOMPILING_EMULATOR=${{ github.workspace }}/qemu/build/qemu-aarch64 \
-DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-toolchain.cmake \
-DCMAKE_C_COMPILER=${toolchain_dir}/bin/clang-19 \
-DCMAKE_CXX_COMPILER=${toolchain_dir}/bin/clang-19 \
-DCMAKE_C_FLAGS="$cross_flags" \
-DCMAKE_CXX_FLAGS="$cross_flags" \
-DCMAKE_EXE_LINKER_FLAGS="$cross_link_flags -lm -Wl,-rpath-link,${rtlibs_dir}/lib" \
-DCMAKE_SHARED_LINKER_FLAGS="$cross_link_flags -Wl,-rpath,${rtlibs_dir}/lib" \
-G Ninja
```
