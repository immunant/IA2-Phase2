# IA2 Phase 2

This repo provides a tool to wrap external libraries with wrappers that use libia2 call gates to keep host applications safe from untrusted libraries.

IA2 Phase 3 will explore other trust models such as mutually-untrusted compartments.

## Setup

Ubuntu 20.04 is used for testing. Other Linux distributions may or may not work. Adjust the commands below accordingly.

### Install the package prerequisites.

```
sudo apt install -y libusb-1.0-0-dev libclang-dev llvm-dev \
            ninja-build zlib1g-dev python3-pip cmake \
            libavformat-dev libavutil-dev pcregrep
pip install lit
```

### Configure with CMake

*Note*: Adjust paths to your version of Clang/LLVM.

```
mkdir build && pushd build
cmake ..                                        \
            -DClang_DIR=/usr/lib/cmake/clang-12 \
            -DLLVM_DIR=/usr/lib/llvm-12/cmake   \
            -DLLVM_EXTERNAL_LIT=`which lit`     \
            -G Ninja
```

### Build and run the tests

*Note*: Pass `-v` to ninja to see build commands and output from failing tests.

```
ninja check-ia2
```

## Usage

### From CMake

IA2 Phase 2 provides a CMake rule to wrap a dependency library; this rule builds a wrapper and provides its dependency information to consumers of its outputs.
Specifically, wrapper libs also depend on libia2 and have additional required compilation flags (-fno-omit-frame-pointer) for application code.

Usage from CMake looks like this (wrapping `myunsafelib` which is used by your existing `my_prog` target):
```diff
+define_ia2_wrapper(
+    WRAPPER my_wrapper_target
+    WRAPPED_LIB myunsafelib-1.0
+    HEADERS myunsafelib.h myunsafelib_config.h
+)
+
 add_executable(my_prog main.c)
-target_link_libraries(my_prog PRIVATE myunsafelib-1.0)
+target_link_libraries(my_prog PRIVATE my_wrapper_target)
```

### Manual usage

Alternatively, you can manually build a wrapper library for a given library:
```
# build your library
$ gcc -g -I../../include/ va_wrap.c -fPIC -shared -o libva_wrap.so
# run the header-rewriter utility to generate output.syms and output.c
$ ...
# compile root.o
$ gcc -g -I ../../include/ -c root.c
# link the wrapper (liboutput.so)
$ gcc -g -fPIC -shared -Wl,--version-script=output.syms output.o -o liboutput.so
```
And then replace references in your build system to the original library (and its headers) with the wrapper (and its headers).
You must also (maybe???) ensure that your trusted code (and the untrusted library?) is built with -fno-omit-frame-pointer.

```
# link your application against both the wrapper and the original library
$ gcc -g output.o -L. -lva_wrap -loutput -o binary
# run your binary
$ LD_LIBRARY_PATH=. ./binary
Hello, World!
```

### Using the Header Rewriter directly
The header rewriter is an out-of-tree clang tool based on LibTooling.
To run it, use the following command after building it:
```
$ /path/to/header-rewriter /path/to/wrapper_output_file.c /path/to/source_1.h /path/to/source_2.h -- -I/usr/lib/clang/A.B.C/include
```

The wrapper library can then be compiled with (assuming the original library is liboriginal.so):
```
$ clang /path/to/wrapper_output_file.c -shared -Wl,--version-script,/path/to/wrapper_output_file.c.syms -loriginal -o libwrapper.so
```

The user application can then link against libwrapper.so using the rewritten
header. For testing you will likely need to add `-Wl,-rpath=path/to/libs` so
that the linker and dynamic loader can find the original and wrapper libraries.
