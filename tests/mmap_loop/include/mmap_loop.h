#pragma once
#include <stdlib.h>

// LINKARGS: --wrap=lib_mmap_buf
char *lib_mmap_buf(size_t size);

// LINKARGS: --wrap=lib_malloc_buf
char *lib_malloc_buf(size_t size);
