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

int dav1d_open(Dav1dContext *c, const Dav1dSettings *s);

void dav1d_close(Dav1dContext *c);

int dav1d_get_picture(Dav1dContext *c, Dav1dPicture *out);
