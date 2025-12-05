#pragma once

#include <string.h>

// Return the basename portion of `path` without modifying the input. Unlike
// POSIX basename(), this helper works with const strings and does not rely on
// static storage, making it safe for concurrent use and for call sites that
// pass string literals.
static inline const char *ia2_basename(const char *path) {
  if (!path) {
    return NULL;
  }
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

