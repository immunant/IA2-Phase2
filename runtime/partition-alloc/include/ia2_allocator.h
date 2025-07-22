#pragma once
#include <stddef.h>

extern "C" {
__attribute__((visibility("default")))
void *shared_malloc(size_t bytes);

__attribute__((visibility("default")))
void shared_free(void *ptr);

__attribute__((visibility("default")))
void *shared_realloc(void *ptr, size_t size);

__attribute__((visibility("default")))
void *shared_calloc(size_t num, size_t size);

__attribute__((visibility("default")))
void *shared_memalign(size_t algin, size_t size);

__attribute__((visibility("default")))
int shared_posix_memalign(void **res, size_t align, size_t size);

__attribute__((visibility("default")))
char *shared_strdup(const char *str);
}
