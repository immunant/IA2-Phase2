/*
 * loader_minimal_malloc - Verify loader heap is tagged with pkey 1
 *
 * This test verifies that the custom glibc loader's minimal malloc heap
 * is protected with MPK pkey 1. The loader sets a VMA name "ia2-loader-heap"
 * so we can find it in /proc/self/smaps and check its ProtectionKey.
 *
 * Requirements:
 *   - MPK-capable CPU and kernel
 *
 * Configure:
 *   cmake -B build -G Ninja -DLIBIA2_REBUILD_GLIBC=ON -DIA2_LIBC_COMPARTMENT=ON
 *
 * Build:
 *   ninja -C build loader_minimal_malloc
 *
 * Run:
 *   ctest --test-dir build -R loader_minimal_malloc --output-on-failure
 */

#include <ia2.h>
#include <ia2_test_runner.h>
#include <ia2_ldso_heap.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
}

/* Marker name set by the loader - must match IA2_LDSO_HEAP_NAME in dl-minimal-malloc.c */

/* Find the loader heap by its unique marker name and verify its pkey.
   The loader sets this name via prctl(PR_SET_VMA_ANON_NAME), bypassing
   the glibc tunable that gates __set_vma_name. */
static int find_loader_malloc_pkey(int *out_pkey) {
  int fd = open("/proc/self/smaps", O_RDONLY);
  if (fd < 0) return -1;

  static char buf[512 * 1024];
  ssize_t total = 0;
  ssize_t n;
  while ((n = read(fd, buf + total, sizeof(buf) - 1 - total)) > 0) {
    total += n;
  }
  close(fd);
  if (total <= 0) return -1;
  buf[total] = '\0';

  /* Find the marker */
  char *marker = strstr(buf, IA2_LDSO_HEAP_MARKER);
  if (!marker) return 2;  /* marker not found */

  /* Find ProtectionKey within this mapping's smaps entry */
  char *pkey_line = strstr(marker, "ProtectionKey:");
  if (!pkey_line) return 1;  /* no pkey line */

  *out_pkey = atoi(pkey_line + 14);
  return 0;
}

Test(loader_minimal_malloc, minimal_malloc_pkey_tag) {
  int pkey = -1;
  int rc = find_loader_malloc_pkey(&pkey);
  if (rc == -1) {
    cr_fatal("failed to read /proc/self/smaps: %s", strerror(errno));
  }
  if (rc == 2) {
    cr_fatal("did not find '" IA2_LDSO_HEAP_MARKER "' in /proc/self/smaps; "
             "is the custom loader with MPK support being used?");
  }
  if (rc == 1) {
    cr_fatal("found loader heap marker but no ProtectionKey line; is MPK enabled?");
  }
  if (pkey != 1) {
    cr_fatal("loader heap has pkey %d, expected 1", pkey);
  }
}
