#define _GNU_SOURCE
#include <sys/wait.h>

#include "memory_map.h"

void track_memory_map(pid_t pid, struct memory_map *map);