// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/thread_isolation/mte.h"

#if BUILDFLAG(ENABLE_MTE_ISOLATION)

#include <errno.h>
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

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
int PkeyMprotect(void *addr, size_t len, int prot, int pkey) {
#ifdef ARCH_CPU_ARM64
  return;
#endif
  return syscall(SYS_pkey_mprotect, addr, len, prot, pkey);
}

void TagMemoryWithMte(int pkey, void *address, size_t size) {
  PA_DCHECK((reinterpret_cast<uintptr_t>(address) &
             PA_THREAD_ISOLATED_ALIGN_OFFSET_MASK) == 0);
#ifdef ARCH_CPU_ARM64
//  return;
#endif
}

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(ENABLE_MTE_ISOLATION)
