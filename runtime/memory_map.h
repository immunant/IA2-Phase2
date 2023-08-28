#pragma once

/* Generated with cbindgen:0.24.6 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
/* clang-format off */

struct memory_map;

struct range {
  size_t start;
  size_t len;
};

struct memory_map *memory_map_new(void);

void memory_map_destroy(struct memory_map *_map);

bool memory_map_all_overlapping_regions_have_pkey(const struct memory_map *map,
                                                  struct range needle,
                                                  uint8_t pkey);

bool memory_map_all_overlapping_regions_pkey_mprotected(const struct memory_map *map,
                                                        struct range needle,
                                                        bool pkey_mprotected);

bool memory_map_unmap_region(struct memory_map *map, struct range needle);

bool memory_map_add_region(struct memory_map *map, struct range range, uint8_t owner_pkey);

bool memory_map_split_region(struct memory_map *map, struct range range, uint8_t owner_pkey);

/* clang-format on */
