#pragma once

#include "ia2_internal.h"

#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define THREAD_NAME_MAX_LEN 16

struct thread_name {
  char name[THREAD_NAME_MAX_LEN];
};

static inline struct thread_name thread_name_get(pthread_t thread) {
  struct thread_name thread_name;
  const int result = pthread_getname_np(thread, thread_name.name, sizeof(thread_name.name));
  if (result == -1) {
    ia2_log("pthread_getname_np failed on thread %ld: %s\n", (long)gettid(), strerrorname_np(result));
    thread_name.name[0] = '?';
    thread_name.name[1] = 0;
  }
  return thread_name;
}
