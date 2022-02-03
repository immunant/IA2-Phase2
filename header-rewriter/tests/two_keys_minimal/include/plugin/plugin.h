/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
*/
// No need to execute the program here since the header exported by the main
// binary does that.
#pragma once

// CHECK: IA2_WRAP_FUNCTION(start_plugin);
void start_plugin(void);
