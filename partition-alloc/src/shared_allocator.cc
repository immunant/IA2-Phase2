#include "base/allocator/partition_allocator/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "base/allocator/partition_allocator/shim/allocator_shim_internals.h"

extern "C" {

SHIM_ALWAYS_EXPORT void *shared_malloc(size_t size) __THROW {
  partition_alloc::ScopedDisallowAllocations guard{};
  return allocator_shim::internal::PartitionAllocMalloc::SharedAllocator()
      ->AllocInline<partition_alloc::AllocFlags::kNoHooks>(size);
}

SHIM_ALWAYS_EXPORT void shared_free(void *object) __THROW {
  partition_alloc::ScopedDisallowAllocations guard{};
  partition_alloc::PartitionRoot::FreeInlineInUnknownRoot<
      partition_alloc::FreeFlags::kNoHooks>(object);
}

SHIM_ALWAYS_EXPORT void *shared_realloc(void *object, size_t size) __THROW {
  partition_alloc::ScopedDisallowAllocations guard{};
  return allocator_shim::internal::PartitionAllocMalloc::SharedAllocator()
      ->Realloc<partition_alloc::AllocFlags::kNoHooks>(object,
                                                       size, "");
}

SHIM_ALWAYS_EXPORT void *shared_calloc(size_t n, size_t size) __THROW {
  partition_alloc::ScopedDisallowAllocations guard{};
  const size_t total =
      partition_alloc::internal::base::CheckMul(n, size).ValueOrDie();
  return allocator_shim::internal::PartitionAllocMalloc::SharedAllocator()
      ->AllocInline<partition_alloc::AllocFlags::kZeroFill |
                    partition_alloc::AllocFlags::kNoHooks>(total);
}
}
