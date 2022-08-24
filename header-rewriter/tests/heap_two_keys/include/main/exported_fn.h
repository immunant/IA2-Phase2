/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: %binary_dir/tests/heap_two_keys/heap_two_keys-main 0 | diff %binary_dir/tests/heap_two_keys/plugin.out -
RUN: %binary_dir/tests/heap_two_keys/heap_two_keys-main 1 | diff %binary_dir/tests/heap_two_keys/main.out -
TODO: %binary_dir/tests/heap_two_keys/heap_two_keys-main 2 | diff %source_dir/tests/heap_two_keys/Output/clean_exit.out -
*/

#pragma once
#include <stdint.h>
#include <stdbool.h>
