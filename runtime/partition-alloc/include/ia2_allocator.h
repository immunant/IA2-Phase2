#pragma once
#include <stddef.h>

void *shared_malloc(size_t bytes);
void shared_free(void *ptr);
void *shared_realloc(void *ptr, size_t size);
void *shared_calloc(size_t num, size_t size);
void *shared_memalign(size_t algin, size_t size);
int shared_posix_memalign(void **res, size_t align, size_t size);
