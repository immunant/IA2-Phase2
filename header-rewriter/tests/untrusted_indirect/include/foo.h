/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %T/wrapper.c.args | FileCheck --check-prefix=LINKARGS %s
RUN: %binary_dir/tests/untrusted_indirect/untrusted_indirect-main | diff %binary_dir/tests/untrusted_indirect/untrusted_indirect.out -
RUN: %binary_dir/tests/untrusted_indirect/untrusted_indirect-main clean_exit | diff %source_dir/tests/untrusted_indirect/Output/untrusted_indirect.clean_exit.out -
*/
#ifndef FOO_H
#define FOO_H
#include <stdbool.h>
#include <stdint.h>

// CHECK: typedef struct IA2_fnptr__ZTSPFmmmE callback_t;
typedef uint64_t(*callback_t)(uint64_t, uint64_t);

// LINKARGS: --wrap=register_callback
bool register_callback(callback_t cb);
// LINKARGS: --wrap=apply_callback
uint64_t apply_callback(uint64_t x, uint64_t y);
// LINKARGS: --wrap=unregister_callback
void unregister_callback();

#endif
