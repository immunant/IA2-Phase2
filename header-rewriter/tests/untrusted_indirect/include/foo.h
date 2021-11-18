/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
RUN: %binary_dir/tests/untrusted_indirect/untrusted_indirect-main | diff %binary_dir/tests/untrusted_indirect/untrusted_indirect.out -
*/
#ifndef FOO_H
#define FOO_H
#include <stdbool.h>
#include <stdint.h>

// CHECK: typedef struct IA2_fnptr__ZTSPFmmmE callback_t;
typedef uint64_t(*callback_t)(uint64_t, uint64_t);

// CHECK: IA2_WRAP_FUNCTION(register_callback);
bool register_callback(callback_t cb);
// CHECK: IA2_WRAP_FUNCTION(apply_callback);
uint64_t apply_callback(uint64_t x, uint64_t y);
// CHECK: IA2_WRAP_FUNCTION(unregister_callback);
void unregister_callback();

#endif
