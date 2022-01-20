/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
RUN: %binary_dir/tests/shared_data/shared_data-main | diff %S/../Output/shared_data.out -
*/

#ifndef ACCESS_SHARED_H
#define ACCESS_SHARED_H
#include <stdint.h>

// CHECK: IA2_WRAP_FUNCTION(read_shared);
void read_shared(uint8_t *shared);

// CHECK: IA2_WRAP_FUNCTION(write_shared);
uint8_t write_shared(uint8_t *shared);

#endif
