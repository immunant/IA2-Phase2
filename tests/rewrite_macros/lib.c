/*
RUN: cat rewrite_macros_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include "lib.h"
#include <stddef.h>

struct event_actions actions = {
    .add = NULL,
    .del = NULL,
    .enable = NULL,
    .disable = NULL,
};

struct event {
  int id;
};

// LINKARGS: --wrap=get_event
struct event *get_event() {
  static struct event evt = {.id = 1};
  return &evt;
}

static bool nop(struct event *evt) {
  return false;
}
static void nop2(struct event *evt) {}

// LINKARGS: --wrap=init_actions
void init_actions() {
  actions.add = nop;
  actions.del = nop;
  actions.enable = nop2;
  actions.disable = nop2;
}
