/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: %binary_dir/tests/tls_protected/tls_protected-main | FileCheck --dump-input=always -v %binary_dir/tests/tls_protected/tls_protected_main.out
RUN: %binary_dir/tests/tls_protected/tls_protected-main print_lib_secret | FileCheck --dump-input=always -v %binary_dir/tests/tls_protected/tls_protected_lib.out
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
