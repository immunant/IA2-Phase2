#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// We treat libsimple1 as a prebuilt in the sense that function pointers are not
// wrapped when received/sent to another compartment. However, because it has
// direct calls to the main binary (which must be wrapped) we link it against
// libsimple1-main-wrapper.so so it's not exactly a prebuilt and requires the
// .symver statements on the hooks.h functions.
#define PREBUILT_LIB
typedef void (*HookFn)(void);

#include "hooks.h"
#include "simple1.h"

static bool did_set_exit_hook = false;

static void simple_exit_hook(void) {
  printf("libsimple exiting...\n");
}

struct Simple {
  struct SimpleCallbacks scb;
  int state;
};

struct Simple *simple_new(struct SimpleCallbacks scb) {
  if (!did_set_exit_hook) {
    set_exit_hook(simple_exit_hook);
    did_set_exit_hook = true;
    printf("New exit hook fn: %p\n", get_exit_hook());
  }

  struct Simple *s = malloc(sizeof(struct Simple));
  if (s == NULL) {
    return NULL;
  }

  s->scb = scb;
  s->state = 0;

  return s;
}

void simple_reset(struct Simple* s) {
  s->state = 0;
}

void simple_destroy(struct Simple* s) {
  free(s);
}

void simple_foreach_v1(struct Simple* s, int (*map_fn)(int)) {
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

void simple_foreach_v2(struct Simple* s, SimpleMapFn map_fn) {
  simple_foreach_v1(s, map_fn);
}
