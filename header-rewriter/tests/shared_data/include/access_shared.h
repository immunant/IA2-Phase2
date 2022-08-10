/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%resource_dir
RUN: cat %t.c.args | FileCheck --check-prefix=LINKARGS %s
RUN: %binary_dir/tests/shared_data/shared_data-main | diff %S/../Output/shared_data.out -
*/

#ifndef ACCESS_SHARED_H
#define ACCESS_SHARED_H
#include <stdint.h>

// LINKARGS: --wrap=read_shared
void read_shared(uint8_t *shared);

// LINKARGS: --wrap=write_shared
uint8_t write_shared(uint8_t *shared, uint8_t new_value);

#endif
