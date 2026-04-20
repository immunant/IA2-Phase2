/*
 * loader_minimal_malloc - Verify the loader heap is not fully retagged shared
 *
 * The custom glibc loader names its anonymous minimal-malloc VMAs
 * "ia2-loader-heap". We scan all such VMAs in /proc/self/smaps and collect
 * their ProtectionKey values.
 *
 * On current IA2 branches we may intentionally retag some loader-heap pages to
 * shared pkey 0 to keep TLS/TCB/DTV state accessible. The useful invariant for
 * this test is therefore not "every loader-heap mapping must be pkey 1", but
 * rather "the loader heap must not collapse entirely to pkey 0". At least one
 * loader-heap mapping should remain compartment-private with pkey 1.
 *
 * Requirements:
 *   - MPK-capable CPU and kernel
 *
 * Configure:
 *   cmake -B build -G Ninja -DIA2_LIBC_COMPARTMENT=ON
 *
 * Build:
 *   ninja -C build loader_minimal_malloc
 *
 * Run:
 *   ctest --test-dir build -R loader_minimal_malloc --output-on-failure
 *   (run the test binary directly to always print the per-mapping pkey
 *    breakdown, even on success)
 */

#include <ia2.h>
#include <ia2_test_runner.h>
#include <ia2_ldso_heap.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
}

/* Marker name set by the loader - must match IA2_LDSO_HEAP_NAME in
   dl-minimal-malloc.c. */

enum {
  MAX_LOADER_HEAP_MAPPINGS = 64,
  BREAKDOWN_BUF_SIZE = 4096,
};

struct LoaderHeapMapping {
  unsigned long start;
  unsigned long end;
  int pkey;
  bool has_pkey;
};

/* Parse /proc/self/smaps and collect every VMA tagged with the loader heap
   marker. */
static int find_loader_malloc_mappings(
    struct LoaderHeapMapping *mappings,
    size_t max_mappings,
    size_t *out_count) {
  FILE *smaps = fopen("/proc/self/smaps", "r");
  if (!smaps) return -1;

  char line[1024];
  bool in_loader_mapping = false;
  size_t count = 0;

  while (fgets(line, sizeof(line), smaps)) {
    unsigned long start = 0;
    unsigned long end = 0;
    char perms[5] = {0};
    unsigned long offset = 0;
    char dev[16] = {0};
    unsigned long inode = 0;
    char path[512] = {0};
    int fields = sscanf(
        line, "%lx-%lx %4s %lx %15s %lu %511[^\n]",
        &start, &end, perms, &offset, dev, &inode, path);
    if (fields >= 6) {
      char *path_start = path;
      while (*path_start == ' ') {
        path_start++;
      }

      in_loader_mapping = false;
      if (fields == 7 && strstr(path_start, IA2_LDSO_HEAP_MARKER) != NULL) {
        if (count >= max_mappings) {
          fclose(smaps);
          return 3;
        }
        mappings[count].start = start;
        mappings[count].end = end;
        mappings[count].pkey = -1;
        mappings[count].has_pkey = false;
        in_loader_mapping = true;
        count++;
      }
      continue;
    }

    if (!in_loader_mapping || count == 0) {
      continue;
    }

    int pkey = -1;
    if (sscanf(line, "ProtectionKey: %d", &pkey) == 1) {
      mappings[count - 1].pkey = pkey;
      mappings[count - 1].has_pkey = true;
    }
  }

  fclose(smaps);
  *out_count = count;
  return count == 0 ? 2 : 0;
}

static void format_loader_heap_breakdown(
    const struct LoaderHeapMapping *mappings,
    size_t count,
    char *buf,
    size_t buf_size) {
  size_t written = 0;
  for (size_t i = 0; i < count; i++) {
    int rc = snprintf(
        buf + written, buf_size - written,
        "%s0x%lx-0x%lx pkey=%d",
        i == 0 ? "" : ", ",
        mappings[i].start, mappings[i].end, mappings[i].pkey);
    if (rc < 0 || (size_t)rc >= buf_size - written) {
      break;
    }
    written += (size_t)rc;
  }
}

Test(loader_minimal_malloc, minimal_malloc_pkey_tag) {
  struct LoaderHeapMapping mappings[MAX_LOADER_HEAP_MAPPINGS] = {0};
  size_t mapping_count = 0;
  int rc = find_loader_malloc_mappings(
      mappings, MAX_LOADER_HEAP_MAPPINGS, &mapping_count);
  if (rc == -1) {
    cr_fatal("failed to read /proc/self/smaps: %s", strerror(errno));
  }
  if (rc == 2) {
    cr_fatal("did not find '" IA2_LDSO_HEAP_MARKER "' in /proc/self/smaps; "
             "is the custom loader with MPK support being used?");
  }
  if (rc == 3) {
    cr_fatal("found more than %d loader heap mappings; increase MAX_LOADER_HEAP_MAPPINGS",
             MAX_LOADER_HEAP_MAPPINGS);
  }

  size_t pkey1_count = 0;
  for (size_t i = 0; i < mapping_count; i++) {
    if (!mappings[i].has_pkey) {
      cr_fatal("loader heap mapping 0x%lx-0x%lx has no ProtectionKey line; "
               "is MPK enabled?",
               mappings[i].start, mappings[i].end);
    }
    if (mappings[i].pkey == 1) {
      pkey1_count++;
    }
  }

  char breakdown[BREAKDOWN_BUF_SIZE] = {0};
  format_loader_heap_breakdown(
      mappings, mapping_count, breakdown, sizeof(breakdown));
  fprintf(stderr, "loader heap pkey breakdown: %s\n", breakdown);

  if (pkey1_count == 0) {
    cr_fatal("all %zu loader heap mappings were retagged away from pkey 1: %s",
             mapping_count, breakdown);
  }
}
