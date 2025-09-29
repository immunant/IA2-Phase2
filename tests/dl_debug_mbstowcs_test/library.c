/*
 * Library compartment that forces glibc's multibyte conversion helpers to load
 * gconv modules via ld.so when called across compartments.
 */

#include "library.h"

#include <ia2.h>
#include <ia2_allocator.h>
#include <ia2_test_runner.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

int trigger_mbstowcs_dlopen(void) {
  const char *phrase = "Gr\303\274\303\237 Gott"; /* "Grüß Gott" */
  size_t capacity = strlen(phrase) + 1;

  char *shared_src = shared_malloc(capacity);
  cr_assert(shared_src);
  memcpy(shared_src, phrase, capacity);

  wchar_t *shared_dst = shared_calloc(capacity, sizeof(wchar_t));
  cr_assert(shared_dst);

  size_t converted = mbstowcs(shared_dst, shared_src, capacity);
  cr_assert(converted != (size_t)-1);
  cr_assert_eq(shared_dst[0], L'G');
  cr_assert_eq(shared_dst[2], L'ü');
  cr_assert_eq(shared_dst[3], L'ß');

  shared_free(shared_dst);
  shared_free(shared_src);
  return 0;
}
