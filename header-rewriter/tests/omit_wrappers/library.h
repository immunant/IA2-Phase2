/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter --omit-wrappers %t.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
*/

#pragma once
#include <stdint.h>

// CHECK: typedef struct IA2_fnptr__ZTSPFjjjE WordFn;
typedef uint32_t (*WordFn)(uint32_t, uint32_t);

typedef struct {
  // CHECK: struct IA2_fnptr__ZTSPFjjjE function;
  uint32_t (*function)(uint32_t, uint32_t);
} HoldsWordFn;

// We aren't emitting wrappers, so this function should not get wrapped
// CHECK-NOT: IA2_WRAP_FUNCTION(not_wrapped)
// CHECK: void not_wrapped();
void not_wrapped();

// This function shouldn't get removed or wrapped
// CHECK-NOT: IA2_WRAP_FUNCTION(untouched_variadics)
// CHECK: void untouched_variadics(int a, ...);
void untouched_variadics(int a, ...);