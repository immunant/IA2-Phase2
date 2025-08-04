#pragma once

#include <stddef.h>

typedef struct Dav1dContext Dav1dContext;

extern const size_t DAV1D_CONTEXT_SIZE;

typedef struct Dav1dSettings {
  int field;
} Dav1dSettings;

typedef struct {
  ptrdiff_t stride[2];
} Dav1dPicture;

void *dav1d_alloc(size_t size);

void dav1d_free(void *memory);

int dav1d_open(Dav1dContext *this, const Dav1dSettings *s);

void dav1d_close(Dav1dContext *this);

int dav1d_get_picture(Dav1dContext *c, Dav1dPicture *out);
