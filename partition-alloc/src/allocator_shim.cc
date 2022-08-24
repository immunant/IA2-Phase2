// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This shim is a simplified version of the one in chromium in
// src/base/allocator/allocator_shim_default_dispatch_to_partition_alloc.cc.

#include "allocator_shim.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include <ia2_get_pkey.h>

extern "C" {

#ifdef LIBIA2_INSECURE
size_t ia2_get_pkey() { return 0; }
#else
size_t ia2_get_pkey() {
  uint32_t pkru;
  __asm__("rdpkru" : "=a"(pkru) : "a"(0), "d"(0), "c"(0));
  switch (pkru) {
  case 0xFFFFFFFC: {
    return 0;
  }
  case 0xFFFFFFF0: {
    return 1;
  }
  case 0xFFFFFFCC: {
    return 2;
  }
  case 0xFFFFFF3C: {
    return 3;
  }
  case 0xFFFFFCFC: {
    return 4;
  }
  case 0xFFFFF3FC: {
    return 5;
  }
  case 0xFFFFCFFC: {
    return 6;
  }
  case 0xFFFF3FFC: {
    return 7;
  }
  case 0xFFFCFFFC: {
    return 8;
  }
  case 0xFFF3FFFC: {
    return 9;
  }
  case 0xFFCFFFFC: {
    return 10;
  }
  case 0xFF3FFFFC: {
    return 11;
  }
  case 0xFCFFFFFC: {
    return 12;
  }
  case 0xF3FFFFFC: {
    return 13;
  }
  case 0xCFFFFFFC: {
    return 14;
  }
  case 0x3FFFFFFC: {
    return 15;
  }
  // TODO: We currently treat any unexpected PKRU value as pkey 0 (the shared
  // heap) for simplicity since glibc(?) initializes the PKRU to 0x55555554
  // (usually). We don't set the PKRU until the first compartment transition, so
  // let's default to using the shared heap before our first wrpkru. When we
  // initialize the PKRU properly (see issue #95) we should probably abort when
  // we see unexpected PKRU values.
  default: {
    return 0;
  }
  }
}
#endif // LIBIA2_INSECURE

using namespace partition_alloc::internal;
using partition_alloc::PartitionOptions;
using partition_alloc::PartitionRoot;
using partition_alloc::internal::PartitionAllocator;

class SimpleScopedSpinLocker {
public:
  explicit SimpleScopedSpinLocker(std::atomic<bool> &lock) : lock_(lock) {
    bool expected = false;
    while (!lock_.compare_exchange_weak(
        expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
      expected = false;
    }
  }

  ~SimpleScopedSpinLocker() { lock_.store(false, std::memory_order_release); }

private:
  std::atomic<bool> &lock_;
};

// This is the LeakySingleton class in
// allocator_shim_default_dispatch_to_partition_alloc.cc, but it's renamed here
// since we don't need it to be generic over the type of partition root.
class SingletonPartition {
public:
  PartitionRoot<ThreadSafe> *Get() {
    auto *instance = instance_.load(std::memory_order_acquire);
    if (instance) {
      return instance->root();
    }
    return GetSlowPath();
  }

private:
  PartitionRoot<ThreadSafe> *GetSlowPath();
  std::atomic<PartitionAllocator<ThreadSafe> *> instance_;
  alignas(PartitionAllocator<ThreadSafe>) uint8_t
      instance_buffer_[sizeof(PartitionAllocator<ThreadSafe>)] = {0};
  std::atomic<bool> initialization_lock_;
};

static PartitionAllocator<ThreadSafe> *NewPartition(void *buffer) {
  auto *new_heap = new (buffer) PartitionAllocator<ThreadSafe>();
  new_heap->init({
      PartitionOptions::AlignedAlloc::kDisallowed,
      PartitionOptions::ThreadCache::kDisabled,
      PartitionOptions::Quarantine::kDisallowed,
      PartitionOptions::Cookie::kDisallowed,
      PartitionOptions::BackupRefPtr::kDisabled,
      PartitionOptions::BackupRefPtrZapping::kDisabled,
      PartitionOptions::UseConfigurablePool::kNo,
  });
  return new_heap;
}

PartitionRoot<ThreadSafe> *SingletonPartition::GetSlowPath() {
  SimpleScopedSpinLocker scoped_lock{initialization_lock_};

  PartitionAllocator<ThreadSafe> *instance =
      instance_.load(std::memory_order_relaxed);

  if (instance) {
    return instance->root();
  }

  instance = ::NewPartition(reinterpret_cast<void *>(instance_buffer_));
  instance_.store(instance, std::memory_order_release);

  return instance->root();
}

SingletonPartition g_partitions[16];

void *ShimMalloc(size_t bytes) {
  size_t pkey = ::ia2_get_pkey();
  return ShimMallocWithPkey(bytes, pkey);
}

void *ShimMallocWithPkey(size_t bytes, size_t pkey) {
  if (pkey == 0) {
    return malloc(bytes);
  } else {
    PartitionRoot<ThreadSafe> *root = g_partitions[pkey].Get();
    return root->Alloc(bytes, nullptr);
  }
}

void ShimFree(void *ptr) {
  size_t pkey = ::ia2_get_pkey();
  ShimFreeWithPkey(ptr, pkey);
}

void ShimFreeWithPkey(void *ptr, size_t pkey) {
  if (pkey == 0) {
    free(ptr);
  } else {
    PartitionRoot<ThreadSafe> *root = g_partitions[pkey].Get();
    root->Free(ptr);
  }
}

void *ShimRealloc(void *ptr, size_t size) {
  size_t pkey = ::ia2_get_pkey();
  return ShimReallocWithPkey(ptr, size, pkey);
}

void *ShimReallocWithPkey(void *ptr, size_t size, size_t pkey) {
  if (pkey == 0) {
    return realloc(ptr, size);
  } else {
    PartitionRoot<ThreadSafe> *root = g_partitions[pkey].Get();
    return root->Realloc(ptr, size, nullptr);
  }
}

void *ShimCalloc(size_t num, size_t size) {
  size_t pkey = ::ia2_get_pkey();
  return ShimCallocWithPkey(num, size, pkey);
}

void *ShimCallocWithPkey(size_t num, size_t size, size_t pkey) {
  if (pkey == 0) {
    return calloc(num, size);
  } else {
    PartitionRoot<ThreadSafe> *root = g_partitions[pkey].Get();
    size_t total;
    if (__builtin_mul_overflow(num, size, &total)) {
      abort();
    }
    void *ret = root->Alloc(total, nullptr);
    if (ret != nullptr) {
      memset(ret, 0, total);
    }
    return ret;
  }
}

} // extern "C"
