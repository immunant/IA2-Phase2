/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %t.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' >/dev/null || FileCheck %s
RUN: %binary_dir/tests/threads/threads-main | FileCheck --dump-input=always -v %binary_dir/tests/threads/threads.out
*/

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
