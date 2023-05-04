/*
RUN: cat %t.c.args | FileCheck --check-prefix=LINKARGS %s
RUN: %binary_dir/tests/global_fn_ptr/global_fn_ptr-main | diff %S/../Output/operations.out -
*/

#pragma once
#include <stdlib.h>
#include <stdint.h>

// LINKARGS: --wrap=call_operations
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
