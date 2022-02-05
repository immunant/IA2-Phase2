# IA2 Phase 2

This repo provides tools for compartmentalizing an application and its dependencies using Intel's memory protection keys (MPK) for userspace. The repo includes a tool that rewrites headers to create call gate wrappers as well as the runtime to initialize the protection keys for trusted compartments.

## Setup

Ubuntu 20.04 is used for testing. Other Linux distributions may or may not work. Adjust the commands below accordingly.

### Install the package prerequisites.

```
sudo apt install -y libusb-1.0-0-dev libclang-dev llvm-dev \
            ninja-build zlib1g-dev python3-pip cmake \
            libavformat-dev libavutil-dev pcregrep
pip install lit
rustup install nightly
```

### Configure with CMake

*Note*: Adjust paths to your version of Clang/LLVM and set `LIBIA2_INSECURE` to `ON` if MPK is not available.

```
mkdir build && pushd build
cmake ..                                        \
            -DClang_DIR=/usr/lib/cmake/clang-12 \
            -DLLVM_DIR=/usr/lib/llvm-12/cmake   \
            -DLLVM_EXTERNAL_LIT=`which lit`     \
            -DLIBIA2_INSECURE=OFF               \
            -G Ninja
```

### Build and run the tests

*Note*: Pass `-v` to ninja to see build commands and output from failing tests.

```
ninja check-ia2
```

## Usage

### Defining compartments

The [`INIT_COMPARTMENT`](https://github.com/immunant/IA2-Phase2/blob/main/include/ia2.h) macro is used to define trusted compartments at the shared object level. This can be the main executable ELF or any dynamically-linked shared libraries. Memory belonging to a trusted compartment is assigned one of the [15 protection keys](https://man7.org/linux/man-pages/man7/pkeys.7.html) and can only be accessed by the shared object itself. Objects that don't explicitly define a compartment are treated as untrusted by default.

To assign a protection key to a trusted compartment insert `INIT_COMPARTMENT(n)` with an argument between 0-14 specifying the index of the protection key. This argument must differ from the other trusted compartments. Trusted compartments must also be aligned and padded properly by using the linker scripts in `libia2/` (`padding.ld` for executables or `shared_lib.ld` for shared libraries). In CMake this is done automatically for executables built with `define_test` while libraries built with `define_shared_lib` must add `LINK_OPTS "-Wl,-T${libia2_BINARY_DIR}/shared_lib.ld"`. To use in manual builds just include `-Wl,-T$SCRIPT_NAME.ld` in the final compilation step. Manual builds also require disabling lazy binding with `-Wl,-z,now`.

### Wrapping calls

Calls between compartments must have call gates to toggle the PKRU permissions at each transition. For direct calls, this is done by rewriting headers to create the source for a wrapper that provides versions of every function with call gates. This wrapper must then be linked against every shared object that links against the wrapped library.

#### From CMake

We provide a CMake rule to wrap a library or the main executable. This rule builds a wrapper and provides its dependency information to consumers of its outputs. Specifically, wrapper libs also depend on libia2 and have additional required compilation flags (-fno-omit-frame-pointer) for application code.

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

Wrapped libraries are treated as untrusted by default. If the library being wrapped defined a trusted compartment, `COMPARTMENT_KEY n` must be specified in define_ia2_wrapper. Here `n` is the argument used in `INIT_COMPARTMENT` to define the compartment. To create a wrapper for the main binary (i.e. if shared libraries call it directly) the `WRAP_MAIN` option must be specified.

#### Manual usage

Alternatively, you can manually build a wrapper library using the header rewriter. The header rewriter is an out-of-tree clang tool based on LibTooling.

To run it, use the following command after building it:
```
$ /path/to/header-rewriter /path/to/wrapper_output_file.c /path/to/source_1.h /path/to/source_2.h -- -I/usr/lib/clang/A.B.C/include
```

If the library being wrapped defined a trusted compartment pass in the `--compartment-key=n` option before the `--`.

The wrapper library can then be compiled with (assuming the original library is liboriginal.so):
```
$ gcc /path/to/wrapper_output_file.c -shared -Wl,--version-script,/path/to/wrapper_output_file.c.syms -loriginal -o libwrapper.so
```

The user application can then link against libwrapper.so using the rewritten
header. For testing you will likely need to add `-Wl,-rpath=path/to/libs` so
that the linker and dynamic loader can find the original and wrapper libraries.

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
