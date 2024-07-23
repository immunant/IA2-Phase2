// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/thread_isolation/mte.h"

#if BUILDFLAG(ENABLE_MTE_ISOLATION)

#include <errno.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/allocator/partition_allocator/partition_alloc_base/cpu.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/thread_isolation/thread_isolation.h"

#if !BUILDFLAG(IS_LINUX)
#error "This MTE code is currently only supported on Linux"
#endif

namespace partition_alloc::internal {

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool CPUHasMteSupport() {
  // Check for Armv8.5-A MTE support, exposed via HWCAP2
  unsigned long hwcap2 = getauxval(AT_HWCAP2);
  return hwcap2 & HWCAP2_MTE;
}

extern "C" int ia2_mprotect_with_tag(void *addr, size_t len, int prot, int tag);

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
int MteMprotect(void* addr, size_t len, int prot, int tag) {
  return ia2_mprotect_with_tag(addr, len, prot, tag);
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void TagMemoryWithMte(int tag, void *address, size_t size) {
  PA_DCHECK((reinterpret_cast<uintptr_t>(address) &
             PA_THREAD_ISOLATED_ALIGN_OFFSET_MASK) == 0);
  PA_PCHECK(ia2_mprotect_with_tag(address, size, PROT_READ|PROT_WRITE, tag) == 0);
}

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(ENABLE_MTE_ISOLATION)
