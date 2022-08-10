/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter --output-header %T/simple1_ia2.h %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %T/wrapper.c.args | FileCheck --check-prefix=LINKARGS %s
*/
#pragma once

struct Simple;

struct SimpleCallbacks {
  // CHECK: IA2_fnptr__ZTSPFiiE read_cb;
  int (*read_cb)(int);
  // CHECK: IA2_fnptr__ZTSPFviE write_cb;
  void (*write_cb)(int);
};

// CHECK: typedef struct IA2_fnptr__ZTSPFiiE SimpleMapFn;
typedef int (*SimpleMapFn)(int);

// LINKARGS: --wrap=simple_new
struct Simple *simple_new(struct SimpleCallbacks);
// LINKARGS: --wrap=simple_reset
void simple_reset(struct Simple*);
// LINKARGS: --wrap=simple_destroy
void simple_destroy(struct Simple*);
// LINKARGS: --wrap=simple_foreach_v1
void simple_foreach_v1(struct Simple*, int (*)(int));
// LINKARGS: --wrap=simple_foreach_v2
void simple_foreach_v2(struct Simple*, SimpleMapFn);

