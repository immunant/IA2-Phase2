#pragma once

/* Generated with cbindgen:0.24.6 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
/* clang-format off */

/**
 * memory_map_region_get_prot found no or multiple protections in the given range
 */
#define MEMORY_MAP_PROT_INDETERMINATE 4294967295u

struct memory_map;

struct range {
  size_t start;
  size_t len;
};

struct memory_map *memory_map_new(void);

void memory_map_destroy(struct memory_map *_map);

bool memory_map_mark_init_finished(struct memory_map *map);

bool memory_map_is_init_finished(const struct memory_map *map);

bool memory_map_all_overlapping_regions_have_pkey(const struct memory_map *map,
                                                  struct range needle,
                                                  uint8_t pkey);

bool memory_map_all_overlapping_regions_pkey_mprotected(const struct memory_map *map,
                                                        struct range needle,
                                                        bool pkey_mprotected);

bool memory_map_all_overlapping_regions_mprotected(const struct memory_map *map,
                                                   struct range needle,
                                                   bool mprotected);

uint32_t memory_map_region_get_prot(const struct memory_map *map, struct range needle);

uint8_t memory_map_region_get_owner_pkey(const struct memory_map *map, struct range needle);

bool memory_map_unmap_region(struct memory_map *map, struct range needle);

bool memory_map_add_region(struct memory_map *map,
                           struct range range,
                           uint8_t owner_pkey,
                           uint32_t prot);

bool memory_map_split_region(struct memory_map *map,
                             struct range range,
                             uint8_t owner_pkey,
                             uint32_t prot);

bool memory_map_pkey_mprotect_region(struct memory_map *map, struct range range, uint8_t pkey);

bool memory_map_mprotect_region(struct memory_map *map, struct range range, uint32_t prot);

void memory_map_clear(struct memory_map *map);

struct memory_map *memory_map_clone(struct memory_map *map);

/* clang-format on */
