#pragma once
#include <stddef.h>

__attribute__((visibility("default"), noinline))
void *shared_malloc(size_t bytes);

__attribute__((visibility("default"), noinline))
void shared_free(void *ptr);

__attribute__((visibility("default"), noinline))
void *shared_realloc(void *ptr, size_t size);

__attribute__((visibility("default"), noinline))
void *shared_calloc(size_t num, size_t size);

__attribute__((visibility("default"), noinline))
void *shared_memalign(size_t algin, size_t size);

__attribute__((visibility("default"), noinline))
int shared_posix_memalign(void **res, size_t align, size_t size);
char* shared_strdup(const char* str);
