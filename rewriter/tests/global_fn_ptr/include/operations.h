/*
*/

#pragma once
#include <stdlib.h>
#include <stdint.h>

uint32_t call_operation(size_t i);

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
