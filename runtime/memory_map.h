#pragma once

#include "mem_region.h"
#include <stdbool.h>

struct mem_region *add_region(struct memory_map *map, struct range range,
                              unsigned char owner_pkey);
struct mem_region *find_overlapping_region(struct memory_map *map,
                                           struct range needle);

struct mem_region *find_region_exact(struct memory_map *map,
                                     struct range needle);

bool remove_region(struct memory_map *map, struct range needle);

struct mem_region *find_region_containing_addr(struct memory_map *map,
                                               size_t addr);

bool all_overlapping_regions_have_pkey(struct memory_map *map,
                                       struct range needle, unsigned char pkey);