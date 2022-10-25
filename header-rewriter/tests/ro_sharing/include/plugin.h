/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: %binary_dir/tests/ro_sharing/ro_sharing-main plugin | diff %binary_dir/tests/ro_sharing/plugin.out -
RUN: %binary_dir/tests/ro_sharing/ro_sharing-main main | diff %binary_dir/tests/ro_sharing/main.out -
*/
#pragma once

#include <stdbool.h>
#include <stdint.h>

const char *get_plugin_str();
const uint32_t *get_plugin_uint(bool secret);
void read_main_string(const char *str);
void read_main_uint(const uint32_t *shared, const uint32_t *secret);
