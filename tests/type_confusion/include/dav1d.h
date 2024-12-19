#pragma once

#include <stddef.h>

typedef struct {
  int field;
} Dav1dContext;

typedef struct Dav1dSettings {
  int field;
} Dav1dSettings;

typedef struct {
  ptrdiff_t stride[2];
} Dav1dPicture;

int dav1d_open(Dav1dContext **const c_out, const Dav1dSettings *const s);

void dav1d_close(Dav1dContext **const c_out);

int dav1d_get_picture(Dav1dContext *c, Dav1dPicture *out);
