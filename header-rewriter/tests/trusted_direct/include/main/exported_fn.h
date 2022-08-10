/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %T/wrapper.c.args | FileCheck --check-prefix=LINKARGS %s
RUN: %binary_dir/tests/trusted_direct/trusted_direct-main | diff %binary_dir/tests/trusted_direct/trusted_direct.out -
RUN: %binary_dir/tests/trusted_direct/trusted_direct-main clean_exit | diff %source_dir/tests/trusted_direct/Output/trusted_direct.clean_exit.out -
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>

// LINKARGS: --wrap=print_message
void print_message(void);

// This is exported to avoid an implicit decl error when the plugin tries to
// access it, but it's explicitly not shared to test that an MPK violation
// occurs.
extern uint32_t secret;

extern bool clean_exit;
