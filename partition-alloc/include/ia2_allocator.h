#pragma once
#include <stddef.h>

void *shared_malloc(size_t bytes);
void shared_free(void *ptr);
void *shared_realloc(void *ptr, size_t size);
void *shared_calloc(size_t num, size_t size);
