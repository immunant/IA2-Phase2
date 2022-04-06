/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
RUN: %binary_dir/tests/global_fn_ptr/global_fn_ptr-main | diff %S/../Output/operations.out -
*/

#pragma once
#include <stdlib.h>
#include <stdint.h>

// CHECK: IA2_WRAP_FUNCTION(call_operations);
void call_operations(void);

// CHECK: typedef struct IA2_fnptr__ZTSPFjjjE WordFn;
typedef uint32_t (*WordFn)(uint32_t, uint32_t);
// CHECK: typedef struct IA2_fnptr__ZTSPFtttE HalfFn;
typedef uint16_t (*HalfFn)(uint16_t, uint16_t);

typedef struct {
    const char *data;
    size_t len;
} str;

typedef struct {
    str name;
    // CHECK: struct IA2_fnptr__ZTSPFjjjE function;
    uint32_t (*function)(uint32_t, uint32_t);
    str desc;
    uint32_t last_result;
} Op;
