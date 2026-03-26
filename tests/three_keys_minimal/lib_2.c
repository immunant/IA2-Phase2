#include <ia2.h>
#include <ia2_test_runner.h>
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

#include "lib.h"
#include "lib_1/lib_1.h"
#include "main/main.h"
#include <ia2_allocator.h>
#include <threads.h>

DEFINE_LIB(2, 1);

/*
 * Regression helper for tracer mmap policy:
 * request a non-fixed mapping with a foreign-compartment hint, then clean it
 * up immediately.
 */
int lib_2_mmap_nonfixed_hint(void *hint) {
  long page_size_l = sysconf(_SC_PAGESIZE);
  if (page_size_l <= 0) {
    return -EINVAL;
  }
  size_t page_size = (size_t)page_size_l;
  uintptr_t aligned_hint = (uintptr_t)hint & ~(page_size - 1);
  void *mapped = mmap((void *)aligned_hint, page_size, PROT_NONE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapped == MAP_FAILED) {
    return -errno;
  }
  if (munmap(mapped, page_size) != 0) {
    return -errno;
  }
  return 0;
}
