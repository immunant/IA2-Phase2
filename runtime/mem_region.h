#pragma once

#include <stddef.h>

#include "memory_map.h"

struct mem_region {
  struct range range;
  unsigned char owner_pkey;
};
