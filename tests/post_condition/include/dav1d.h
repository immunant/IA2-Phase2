#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  int field;
} Dav1dContext;

typedef struct {
  ptrdiff_t stride[2];
} Dav1dPicture;

extern bool corrupt_stride;

int dav1d_get_picture(Dav1dContext *c, Dav1dPicture *out);
