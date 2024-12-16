#pragma once
#include <stdint.h>
#include <threads.h>

thread_local extern uint32_t main_secret;
thread_local extern uint32_t lib_secret;

void lib_print_main_secret(void);

void lib_print_lib_secret(void);
