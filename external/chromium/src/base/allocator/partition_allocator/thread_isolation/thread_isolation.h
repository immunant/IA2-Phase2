// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_ISOLATION_THREAD_ISOLATION_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_ISOLATION_THREAD_ISOLATION_H_

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)

#include <array>
#include <cstddef>
#include <cstdint>

#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"

#if BUILDFLAG(ENABLE_PKEYS)
#include "base/allocator/partition_allocator/thread_isolation/pkey.h"
#endif

#if BUILDFLAG(ENABLE_MTE_ISOLATION)
#include "base/allocator/partition_allocator/thread_isolation/mte.h"
#endif

#if !BUILDFLAG(HAS_64_BIT_POINTERS)
#error "thread isolation support requires 64 bit pointers"
#endif

namespace partition_alloc {

using Compartment = size_t;

struct ThreadIsolationOption {
  constexpr ThreadIsolationOption() = default;
  explicit ThreadIsolationOption(bool enabled) : enabled(enabled) {}

#if BUILDFLAG(ENABLE_PKEYS) || BUILDFLAG(ENABLE_MTE_ISOLATION)
  explicit ThreadIsolationOption(int pkey, size_t compartment)
      : pkey(pkey), compartment(compartment) {
    enabled = pkey != internal::kInvalidPkey;
  }
  int pkey = -1;
  Compartment compartment = 0;
#endif  // BUILDFLAG(ENABLE_PKEYS) || BUILDFLAG(ENABLE_MTE_ISOLATION)

  bool enabled = false;

  bool operator==(const ThreadIsolationOption& other) const {
#if BUILDFLAG(ENABLE_PKEYS) || BUILDFLAG(ENABLE_MTE_ISOLATION)
    if (pkey != other.pkey) {
      return false;
    }
#endif  // BUILDFLAG(ENABLE_PKEYS) || BUILDFLAG(ENABLE_MTE_ISOLATION)
    return enabled == other.enabled;
  }
};

}  // namespace partition_alloc

namespace partition_alloc::internal {

#if BUILDFLAG(PA_DCHECK_IS_ON)

struct PA_THREAD_ISOLATED_ALIGN ThreadIsolationSettings {
  bool enabled = false;
  static ThreadIsolationSettings settings PA_CONSTINIT;
};

#if BUILDFLAG(ENABLE_PKEYS)

using LiftThreadIsolationScope = DoNotLiftPkeyRestrictionsScope;

#elif BUILDFLAG(ENABLE_MTE_ISOLATION)

using LiftThreadIsolationScope = void*;

#endif  // BUILDFLAG(ENABLE_PKEYS)

#endif  // BUILDFLAG(PA_DCHECK_IS_ON)

void WriteProtectThreadIsolatedGlobals(
    std::array<ThreadIsolationOption, kNumCompartments> &thread_isolations);
void WriteProtectThreadIsolatedGlobals(ThreadIsolationOption thread_isolation);
void UnprotectThreadIsolatedGlobals();
[[nodiscard]] int MprotectWithThreadIsolation(
    void* addr,
    size_t len,
    int prot,
    ThreadIsolationOption thread_isolation);

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(ENABLE_THREAD_ISOLATION)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_ISOLATION_THREAD_ISOLATION_H_
