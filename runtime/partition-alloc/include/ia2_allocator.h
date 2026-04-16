#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__)
#define IA2_STRINGIFY_IMPL(x) #x
#define IA2_STRINGIFY(x) IA2_STRINGIFY_IMPL(x)
#define IA2_EXTERN_PKEY_ANNOTATION(pkey) \
  __attribute__((annotate("ia2_extern_pkey:" IA2_STRINGIFY(pkey))))
#else
#define IA2_EXTERN_PKEY_ANNOTATION(pkey)
#endif

__attribute__((visibility("default"), noinline))
IA2_EXTERN_PKEY_ANNOTATION(1)
void *shared_malloc(size_t bytes);

__attribute__((visibility("default"), noinline))
IA2_EXTERN_PKEY_ANNOTATION(1)
void shared_free(void *ptr);

__attribute__((visibility("default"), noinline))
IA2_EXTERN_PKEY_ANNOTATION(1)
void *shared_realloc(void *ptr, size_t size);

__attribute__((visibility("default"), noinline))
IA2_EXTERN_PKEY_ANNOTATION(1)
void *shared_calloc(size_t num, size_t size);

__attribute__((visibility("default"), noinline))
IA2_EXTERN_PKEY_ANNOTATION(1)
void *shared_memalign(size_t algin, size_t size);

__attribute__((visibility("default"), noinline))
IA2_EXTERN_PKEY_ANNOTATION(1)
int shared_posix_memalign(void **res, size_t align, size_t size);

__attribute__((visibility("default"), noinline))
IA2_EXTERN_PKEY_ANNOTATION(1)
char* shared_strdup(const char* str);

#undef IA2_EXTERN_PKEY_ANNOTATION
#undef IA2_STRINGIFY
#undef IA2_STRINGIFY_IMPL

#ifdef __cplusplus
} // extern "C"
#endif
