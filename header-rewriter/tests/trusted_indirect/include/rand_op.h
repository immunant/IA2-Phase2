/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
RUN: %binary_dir/tests/trusted_indirect/trusted_indirect-main | diff %binary_dir/tests/trusted_indirect/trusted_indirect.out -
RUN: %binary_dir/tests/trusted_indirect/trusted_indirect-main clean_exit | diff %source_dir/tests/trusted_indirect/Output/trusted_indirect.clean_exit.out -
*/
#pragma once
#include <stdint.h>

// CHECK: typedef struct IA2_fnptr__ZTSPFjjjE binary_op;
typedef uint32_t(*binary_op)(uint32_t, uint32_t);

typedef struct function_s {
    binary_op op;
    const char *name;
} function_t;

// CHECK: IA2_WRAP_FUNCTION(swap_function);
void swap_function(void);

// CHECK: IA2_WRAP_FUNCTION(get_function);
function_t get_function(void);

// CHECK: IA2_WRAP_FUNCTION(sub);
uint32_t sub(uint32_t x, uint32_t y);

// CHECK: IA2_WRAP_FUNCTION(get_bad_function);
function_t get_bad_function(void);

// CHECK: IA2_WRAP_FUNCTION(leak_secret_address);
void leak_secret_address(uint32_t *addr);
