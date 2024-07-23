// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_ISOLATION_MTE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_ISOLATION_MTE_H_

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"

#if BUILDFLAG(ENABLE_MTE_ISOLATION)

#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/thread_isolation/alignment.h"

#include <cstddef>
#include <cstdint>

namespace partition_alloc::internal {

constexpr int kDefaultPkey = 0;
constexpr int kInvalidPkey = -1;

// Check if the CPU supports pkeys.
bool CPUHasMteSupport();

int MteMprotect(void* addr, size_t len, int prot, int tag);

void TagMemoryWithMte(int tag, void* address, size_t size);

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(ENABLE_MTE_ISOLATION)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_ISOLATION_MTE_H_
