#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#pragma once

typedef void (*Fn)(void);

// This function does nothing, but should get wrapped
// LINKARGS: --wrap=library_foo
void library_foo();

// LINKARGS: --wrap=library_spawn_thread
pthread_t library_spawn_thread(void);

// LINKARGS: --wrap=library_showpkru
void library_showpkru(void);

// effectively just memset, but from within the compartment
// LINKARGS: --wrap=library_memset
void library_memset(void *ptr, uint8_t byte, size_t n);

// LINKARGS: --wrap=library_call_fn
void library_call_fn(Fn what);

// LINKARGS: --wrap=library_access_int_ptr
int library_access_int_ptr(int *ptr);
