/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: %binary_dir/tests/string_literals/string_literals-main plugin | diff %binary_dir/tests/string_literals/plugin.out -
RUN: %binary_dir/tests/string_literals/string_literals-main main | diff %binary_dir/tests/string_literals/main.out -
*/
#pragma once

const char *get_plugin_shared_str();
const char *get_plugin_secret_str();
void read_main_strings(const char *shared, const char *secret);
