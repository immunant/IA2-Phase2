#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#pragma once

typedef void (*Fn)(void);

// This function does nothing, but should get wrapped
void library_foo();

pthread_t library_spawn_thread(void);

void library_showpkru(void);

// effectively just memset, but from within the compartment
void library_memset(void *ptr, uint8_t byte, size_t n);

void library_call_fn(Fn what);

int library_access_int_ptr(int *ptr);
