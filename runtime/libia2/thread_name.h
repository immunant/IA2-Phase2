#pragma once

#include <pthread.h>

#define THREAD_NAME_MAX_LEN 16

struct thread_name {
  char name[THREAD_NAME_MAX_LEN];
};

struct thread_name thread_name_get(pthread_t thread);
