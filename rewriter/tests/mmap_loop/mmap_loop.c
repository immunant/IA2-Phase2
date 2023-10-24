
#include "mmap_loop.h"
#include "ia2.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

char *lib_malloc_buf(size_t size) {
  char *buf = malloc(size);
  memset(buf, 0, size);
  return buf;
}

char *lib_mmap_buf(size_t size) {
  // mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  char *buf = mmap(NULL, size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
  pkey_mprotect(buf, size, PROT_READ | PROT_WRITE, 2);
  memset(buf, 0, size);
  return buf;
}
