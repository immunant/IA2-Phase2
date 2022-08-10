/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %T/wrapper.c.args | FileCheck --check-prefix=LINKARGS %s
*/
// No need to execute the program here since the header exported by the main
// binary does that.
#pragma once

// LINKARGS: --wrap=start_plugin
void start_plugin(void);
