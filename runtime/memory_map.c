// #include "mem_map.h"
#include "memory_map.h"
#include <stdio.h>
#include <stdlib.h>

struct mem_region *add_region(struct memory_map *map, struct range range,
                              unsigned char owner_pkey) {
  printf("adding compartment %d region %08zx+%zd\n", owner_pkey, range.start,
         range.len);

  if (map->n_regions == map->capacity) {
    map->capacity *= 2;
    if (map->capacity == 0) {
      map->capacity = 16;
    }
    map->regions =
        realloc(map->regions, map->capacity * sizeof(struct mem_region));
  }

  map->n_regions++;

  struct mem_region *region = &map->regions[map->n_regions - 1];
  region->range = range;
  region->owner_pkey = owner_pkey;

  return region;
}

bool ranges_overlap(struct range *a, struct range *b) {
  size_t a_end = a->start + a->len;
  size_t b_end = b->start + b->len;
  return a_end >= b->start && b_end >= a->start;
}

struct mem_region *find_overlapping_region(struct memory_map *map,
                                           struct range needle) {
  for (int i = 0; i < map->n_regions; i++) {
    struct mem_region *region = &map->regions[i];
    if (ranges_overlap(&region->range, &needle)) {
      return region;
    }
  }
  return NULL;
}

struct mem_region *find_region_exact(struct memory_map *map,
                                     struct range needle) {
  for (int i = 0; i < map->n_regions; i++) {
    struct mem_region *region = &map->regions[i];
    if (region->range.start == needle.start &&
        region->range.len == needle.len) {
      return region;
    }
  }
  return NULL;
}

bool remove_region(struct memory_map *map, struct range needle) {
  struct mem_region *r = find_region_exact(map, needle);
  if (r != NULL) {
    /* move the last region here */
    *r = map->regions[map->n_regions - 1];
    /* pop the old last region */
    map->n_regions--;
    return true;
  }
  return false;
}

struct mem_region *find_region_containing_addr(struct memory_map *map,
                                               size_t addr) {
  struct range needle = {addr, addr};
  return find_overlapping_region(map, needle);
}

bool all_overlapping_regions_have_pkey(struct memory_map *map,
                                       struct range needle,
                                       unsigned char pkey) {
  for (int i = 0; i < map->n_regions; i++) {
    struct mem_region *region = &map->regions[i];
    bool pkeys_differ = region->owner_pkey != pkey;
    printf("  pkeys: %d/%d, ranges: %zx+%zd, %zx+%zd\n", region->owner_pkey,
           pkey, region->range.start, region->range.len, needle.start,
           needle.len);
    if (pkeys_differ && ranges_overlap(&region->range, &needle)) {
      printf("  ^ counterexample!\n");
      return false;
    }
  }
  return true;
}
