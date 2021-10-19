# IA2 Phase 2

## Setup

Ubuntu 20.04 is used for testing. Other Linux distributions may or may not work. Adjust the commands below accordingly.

### Install the package prerequisites.

```
sudo apt install -y libusb-1.0-0-dev libclang-dev llvm-dev \
            ninja-build zlib1g-dev python3-pip cmake
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

## Header Rewriter
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
