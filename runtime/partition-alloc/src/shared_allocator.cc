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

SHIM_ALWAYS_EXPORT void *shared_memalign(size_t alignment, size_t size) __THROW {
  partition_alloc::ScopedDisallowAllocations guard{};
  // Memory returned by the regular allocator *always* respects |kAlignment|,
  // which is a power of two, and any valid alignment is also a power of two. So
  // we can directly fulfill these requests with the main allocator.
  //
  // This has several advantages:
  // - The thread cache is supported on the main partition
  // - Reduced fragmentation
  // - Better coverage for MiraclePtr variants requiring extras
  //
  // There are several call sites in Chromium where base::AlignedAlloc is called
  // with a small alignment. Some may be due to overly-careful code, some are
  // because the client code doesn't know the required alignment at compile
  // time.
  //
  // Note that all "AlignedFree()" variants (_aligned_free() on Windows for
  // instance) directly call PartitionFree(), so there is no risk of
  // mismatch. (see below the default_dispatch definition).
  if (alignment <= partition_alloc::internal::kAlignment) {
    // This is mandated by |posix_memalign()| and friends, so should never fire.
    PA_CHECK(partition_alloc::internal::base::bits::IsPowerOfTwo(alignment));
    // TODO(bartekn): See if the compiler optimizes branches down the stack on
    // Mac, where PartitionPageSize() isn't constexpr.
    return allocator_shim::internal::PartitionAllocMalloc::SharedAllocator()
        ->AllocInline<partition_alloc::AllocFlags::kNoHooks>(size);
  }

  return allocator_shim::internal::PartitionAllocMalloc::SharedAllocator()
      ->AlignedAllocInline<partition_alloc::AllocFlags::kNoHooks>(alignment,
                                                                  size);
}

SHIM_ALWAYS_EXPORT int shared_posix_memalign(void **res, size_t alignment, size_t size) __THROW {
  // posix_memalign is supposed to check the arguments. See tc_posix_memalign()
  // in tc_malloc.cc.
  if (((alignment % sizeof(void *)) != 0) ||
      !partition_alloc::internal::base::bits::IsPowerOfTwo(alignment)) {
    return EINVAL;
  }
  void *ptr = shared_memalign(alignment, size);
  *res = ptr;
  return ptr ? 0 : ENOMEM;
}
}
