/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
RUN: %binary_dir/tests/two_keys_minimal/two_keys_minimal-main plugin | diff %binary_dir/tests/two_keys_minimal/plugin.out -
RUN: %binary_dir/tests/two_keys_minimal/two_keys_minimal-main main | diff %binary_dir/tests/two_keys_minimal/main.out -
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>

// CHECK: IA2_WRAP_FUNCTION(print_message);
void print_message(void);

// This is exported to avoid an implicit decl error when the plugin tries to
// access it, but it's explicitly not shared to test that an MPK violation
// occurs.
extern uint32_t secret;

extern bool debug_mode;
