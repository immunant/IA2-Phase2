#pragma once

struct Simple;

struct SimpleCallbacks {
  int (*read_cb)(int);
  void (*write_cb)(int);
};

typedef int (*SimpleMapFn)(int);

struct Simple *simple_new(struct SimpleCallbacks);
void simple_reset(struct Simple*);
void simple_destroy(struct Simple*);
void simple_foreach_v1(struct Simple*, int (*)(int));
void simple_foreach_v2(struct Simple*, SimpleMapFn);

