#pragma once

#include <stddef.h>

struct range {
  size_t start;
  size_t len;
};

struct mem_region {
  struct range range;
  unsigned char owner_pkey;
};

// maps regions to pkeys
struct memory_map {
  struct mem_region *regions;
  size_t n_regions;
  size_t capacity;
};
