#pragma once

#if __has_include("simple1_ia2.h")
#include "simple1_ia2.h"
#endif

struct Simple;

struct SimpleCallbacks {
  int (*read_cb)(int);
  void (*write_cb)(int);
};

typedef int (*SimpleMapFn)(int);

struct Simple *simple_new(struct SimpleCallbacks);
void simple_destroy(struct Simple*);
void simple_foreach(struct Simple*, SimpleMapFn);

