/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %T/wrapper.c.args | FileCheck --check-prefix=LINKARGS %s
RUN: %binary_dir/tests/should_segfault/should_segfault-main | diff %binary_dir/tests/should_segfault/should_segfault.out -
RUN: %binary_dir/tests/should_segfault/should_segfault-main early_fault | diff %binary_dir/tests/should_segfault/early_segfault.out -
*/
#pragma once
#include <stdint.h>

extern uint32_t secret;

// LINKARGS: --wrap=print_secret
void print_secret();

// LINKARGS: --wrap=do_early_fault
void do_early_fault();
