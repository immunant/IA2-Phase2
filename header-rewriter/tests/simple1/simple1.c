#include <stdio.h>
#include <stdlib.h>

#include "simple1.h"

struct Simple {
  struct SimpleCallbacks scb;
  int state;
};

struct Simple *simple_new(struct SimpleCallbacks scb) {
  struct Simple *s = malloc(sizeof(struct Simple));
  if (s == NULL) {
    return NULL;
  }

  s->scb = scb;
  s->state = 0;

  return s;
}

void simple_destroy(struct Simple* s) {
  free(s);
}

void simple_foreach(struct Simple* s, SimpleMapFn map_fn) {
  for (;;) {
    int value = (*s->scb.read_cb)(s->state);
    if (value == 0) {
      break;
    }

    value = (*map_fn)(value);
    (*s->scb.write_cb)(value);

    s->state++;
  }
}
