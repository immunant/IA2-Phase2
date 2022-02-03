/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
*/
// No need to execute the program here since the header exported by the main
// binary does that.
#pragma once
#include <stdint.h>

// CHECK: IA2_WRAP_FUNCTION(start_plugin);
void start_plugin(void);

// This is exported to avoid an implicit decl error when the main binary tries
// to access it, but it's explicitly not shared to test that an MPK violation
// occurs.
extern uint32_t plugin_secret;
