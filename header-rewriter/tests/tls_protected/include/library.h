/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
RUN: %binary_dir/tests/tls_protected/tls_protected-main | diff %binary_dir/tests/tls_protected/tls_protected_main.out -
RUN: %binary_dir/tests/tls_protected/tls_protected-main print_lib_secret | diff %binary_dir/tests/tls_protected/tls_protected_lib.out -
*/
#pragma once
#include <stdint.h>
#include <threads.h>

thread_local extern uint32_t main_secret;
thread_local extern uint32_t lib_secret;

// CHECK: IA2_WRAP_FUNCTION(lib_print_main_secret);
void lib_print_main_secret();

// CHECK: IA2_WRAP_FUNCTION(lib_print_lib_secret);
void lib_print_lib_secret();
