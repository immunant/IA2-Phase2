#pragma once

#include <features.h>
#include <malloc.h>
#include <unistd.h>

#ifndef __THROW  // Not a glibc system
#ifdef _NOEXCEPT // LLVM libc++ uses noexcept instead
#define __THROW _NOEXCEPT
#else
#define __THROW
#endif
#endif

#define SHIM_ALWAYS_EXPORT __attribute__((visibility("default"), noinline))

#ifdef __cplusplus
extern "C" {
#endif

void *ShimMalloc(size_t bytes);
void ShimFree(void *ptr);
void *ShimRealloc(void *ptr, size_t size);
void *ShimCalloc(size_t num, size_t size);

void *ShimMallocWithPkey(size_t bytes, size_t pkey);
void ShimFreeWithPkey(void *ptr, size_t pkey);
void *ShimReallocWithPkey(void *ptr, size_t size, size_t pkey);
void *ShimCallocWithPkey(size_t num, size_t size, size_t pkey);

SHIM_ALWAYS_EXPORT void *__wrap_malloc(size_t size) __THROW {
  return ShimMalloc(size);
}

SHIM_ALWAYS_EXPORT void __wrap_free(void *ptr) __THROW { ShimFree(ptr); }

SHIM_ALWAYS_EXPORT void *__wrap_realloc(void *ptr, size_t size) __THROW {
  return ShimRealloc(ptr, size);
}

SHIM_ALWAYS_EXPORT void *__wrap_calloc(size_t num, size_t size) __THROW {
  return ShimCalloc(num, size);
}

SHIM_ALWAYS_EXPORT void *shared_malloc(size_t size) __THROW {
  return ShimMallocWithPkey(size, 0);
}

SHIM_ALWAYS_EXPORT void shared_free(void *ptr) __THROW {
  ShimFreeWithPkey(ptr, 0);
}

SHIM_ALWAYS_EXPORT void *shared_realloc(void *ptr, size_t size) __THROW {
  return ShimReallocWithPkey(ptr, size, 0);
}

SHIM_ALWAYS_EXPORT void *shared_calloc(size_t num, size_t size) __THROW {
  return ShimCallocWithPkey(num, size, 0);
}

#ifdef __cplusplus
} /* extern "C" */
#endif
