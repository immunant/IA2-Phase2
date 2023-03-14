/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %T/wrapper.c.args | FileCheck --check-prefix=LINKARGS %s
*/

#pragma once

// LINKARGS: --wrap=recurse_dso
void recurse_dso(int count);
