# Setup

Ubuntu 20.04 is used for testing. Other Linux distributions may or may not work. Adjust the commands below accordingly.

## Install the package prerequisites.

```
sudo apt install -y libusb-1.0-0-dev libclang-dev llvm-dev \
            ninja-build zlib1g-dev python3-pip cmake \
            libavformat-dev libavutil-dev pcregrep patchelf
pip install lit
rustup install nightly
```

## Configure with CMake

*Note*: Adjust paths to your version of Clang/LLVM

```
mkdir build && pushd build
cmake ..                                        \
            -DClang_DIR=/usr/lib/cmake/clang-12 \
            -DLLVM_DIR=/usr/lib/llvm-12/cmake   \
            -DLLVM_EXTERNAL_LIT=`which lit`     \
            -G Ninja
```

## Build and run the tests

*Note*: Pass `-v` to ninja to see build commands and output from failing tests.

```
ninja check-ia2
```
