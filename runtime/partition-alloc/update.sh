#!/bin/bash

set -Eeuo pipefail

REPO_ROOT=$(pwd)
REPO_CHROMIUM=$REPO_ROOT/external/chromium
REPO_SRC=$REPO_CHROMIUM/src
NEW_CHROMIUM=$REPO_ROOT/chromium
NEW_SRC=$NEW_CHROMIUM/src
DEPOT_TOOLS=$REPO_ROOT/depot_tools

# Download depot_tools
if [[ -d "$DEPOT_TOOLS" ]];
then
	  rm -rf $DEPOT_TOOLS
fi
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git $DEPOT_TOOLS
export PATH=$PATH:$DEPOT_TOOLS

# Download chromium
if [[ -d "$NEW_CHROMIUM" ]];
then
	  rm -rf $NEW_CHROMIUM
fi
mkdir $NEW_CHROMIUM
pushd $NEW_CHROMIUM
fetch --nohooks --no-history chromium
pushd src
git log -1 --format="%H" > $REPO_ROOT/chromium_commit
# ./build/install-build-deps.sh
gclient runhooks
gn gen out/Default
autoninja -C out/Default base/allocator/partition_allocator:partition_alloc
git apply $REPO_ROOT/scripts/partition_alloc/partition_alloc.diff

popd
popd

if [[ -d "$REPO_CHROMIUM" ]];
then
	  rm -rf $REPO_CHROMIUM
fi

mkdir -p $REPO_SRC

mkdir $REPO_SRC/build
cp $NEW_SRC/build/build_config.h $NEW_SRC/build/buildflag.h $NEW_SRC/build/precompile.h $REPO_SRC/build
mkdir -p $REPO_SRC/testing/gtest/include/gtest
cp $NEW_SRC/testing/gtest/include/gtest/gtest_prod.h $REPO_SRC/testing/gtest/include/gtest
mkdir -p $REPO_SRC/third_party/googletest/src/googletest/include/gtest
cp $NEW_SRC/third_party/googletest/src/googletest/include/gtest/gtest_prod.h $REPO_SRC/third_party/googletest/src/googletest/include/gtest
mkdir -p $REPO_SRC/third_party/lss
cp $NEW_SRC/third_party/lss/linux_syscall_support.h $REPO_SRC/third_party/lss
mkdir -p $REPO_SRC/base/allocator
cp -r $NEW_SRC/base/allocator/partition_allocator $REPO_SRC/base/allocator

# Copy over the generated headers
OUT_PARTITION_ALLOC=$NEW_SRC/out/Default/gen/base/allocator/partition_allocator
REPO_PARTITION_ALLOC=$REPO_SRC/base/allocator/partition_allocator

cp $OUT_PARTITION_ALLOC/partition_alloc_base/debug/debugging_buildflags.h $REPO_PARTITION_ALLOC/partition_alloc_base/debug
cp $OUT_PARTITION_ALLOC/partition_alloc_buildflags.h $REPO_PARTITION_ALLOC
cp $OUT_PARTITION_ALLOC/chromeos_buildflags.h $REPO_PARTITION_ALLOC
cp $OUT_PARTITION_ALLOC/chromecast_buildflags.h $REPO_PARTITION_ALLOC
cp $OUT_PARTITION_ALLOC/logging_buildflags.h $REPO_PARTITION_ALLOC

rm -rf $NEW_CHROMIUM
rm -rf $DEPOT_TOOLS

