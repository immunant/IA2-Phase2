#pragma once
#include <stdint.h>

// CHECK: typedef struct IA2_fnptr__ZTSPFjjjE binary_op;
typedef uint32_t(*binary_op)(uint32_t, uint32_t);

typedef struct function_s {
    binary_op op;
    const char *name;
} function_t;

void swap_function(void);

function_t get_function(void);

uint32_t sub(uint32_t x, uint32_t y);

function_t get_bad_function(void);

void leak_secret_address(uint32_t *addr);
