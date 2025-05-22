#include "memory_maps.h"
#include "ia2.h"

// Only enable this code that stores these addresses when debug logging is enabled.
// This reduces the trusted codebase and avoids runtime overhead.
#if IA2_DEBUG_MEMORY

// It's much simpler to only support a static number of created threads,
// especially because we want to have very few dependencies.
// If a program needs more threads, you can just increase this number.
#define MAX_THREADS 512

struct ia2_all_threads_metadata {
  pthread_mutex_t lock;
  size_t num_threads;
  pid_t tids[MAX_THREADS];
  struct ia2_thread_metadata thread_metadata[MAX_THREADS];
};

#define array_len(a) (sizeof(a) / sizeof(*(a)))

struct ia2_thread_metadata *ia2_all_threads_metadata_lookup(struct ia2_all_threads_metadata *const this) {
  const pid_t tid = gettid();

  struct ia2_thread_metadata *metadata = NULL;
  if (pthread_mutex_lock(&this->lock) != 0) {
    perror("pthread_mutex_lock in ia2_all_threads_data_lookup failed");
    goto ret;
  }
  for (size_t i = 0; i < this->num_threads; i++) {
    if (this->tids[i] == tid) {
      metadata = &this->thread_metadata[i];
      goto unlock;
    }
  }
  if (this->num_threads >= array_len(this->thread_metadata)) {
    fprintf(stderr, "created %zu threads, but can't store them all (max is MAX_THREADS)\n", this->num_threads);
    goto unlock;
  }

  metadata = &this->thread_metadata[this->num_threads];
  this->tids[this->num_threads] = tid;
  this->num_threads++;

  metadata->tid = tid;
  metadata->thread = pthread_self();

  goto unlock;

unlock:
  if (pthread_mutex_unlock(&this->lock) != 0) {
    perror("pthread_mutex_unlock in ia2_all_threads_data_lookup failed");
  }
ret:
  return metadata;
}

struct ia2_addr_location ia2_all_threads_metadata_find_addr(struct ia2_all_threads_metadata *const this, const uintptr_t addr) {
  struct ia2_addr_location location = {
      .name = NULL,
      .tid = -1,
      .compartment = -1,
  };
  if (pthread_mutex_lock(&this->lock) != 0) {
    perror("pthread_mutex_lock in ia2_all_threads_data_find_addr failed");
    goto ret;
  }
  for (size_t thread = 0; thread < this->num_threads; thread++) {
    const pid_t tid = this->tids[thread];
    for (int compartment = 0; compartment < IA2_MAX_COMPARTMENTS; compartment++) {
      const struct ia2_thread_metadata *const thread_metadata = &this->thread_metadata[thread];
      if (addr == thread_metadata->stack_addrs[compartment]) {
        location.name = "stack";
        location.tid = tid;
        location.thread = thread_metadata->thread;
        location.compartment = compartment;
        goto unlock;
      }
      if (addr == thread_metadata->tls_addrs[compartment]) {
        location.name = "tls";
        location.tid = tid;
        location.thread = thread_metadata->thread;
        location.compartment = compartment;
        goto unlock;
      }
      if (addr == thread_metadata->tls_addr_compartment1_first || addr == thread_metadata->tls_addr_compartment1_second) {
        location.name = "tls";
        location.tid = tid;
        location.thread = thread_metadata->thread;
        location.compartment = 1;
        goto unlock;
      }
    }
  }

  goto unlock;

unlock:
  if (pthread_mutex_unlock(&this->lock) != 0) {
    perror("pthread_mutex_unlock in ia2_all_threads_data_find_addr failed");
  }
ret:
  return location;
}

// All zeroed, so this should go in `.bss` and only have pages lazily allocated.
static struct ia2_all_threads_metadata IA2_SHARED_DATA threads = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .num_threads = 0,
    .thread_metadata = {0},
};

struct ia2_thread_metadata *ia2_thread_metadata_get_current_thread(void) {
  return ia2_all_threads_metadata_lookup(&threads);
}

struct ia2_addr_location ia2_addr_location_find(const uintptr_t addr) {
  return ia2_all_threads_metadata_find_addr(&threads, addr);
}

extern uintptr_t (*partition_alloc_thread_isolated_pool_base_address)[IA2_MAX_COMPARTMENTS];

static void label_memory_map(FILE *log, uintptr_t start_addr) {
  const struct ia2_addr_location location = ia2_addr_location_find(start_addr);
  if (location.name) {
    char thread_name[16] = {0};
    const bool has_thread_name = pthread_getname_np(location.thread, thread_name, sizeof(thread_name)) == 0;
    fprintf(log, "[%s:tid %ld", location.name, (long)location.tid);
    if (has_thread_name) {
      fprintf(log, " (thread %s)", thread_name);
    }
    fprintf(log, ":compartment %d]", location.compartment);
  }
  if (partition_alloc_thread_isolated_pool_base_address) {
    for (size_t pkey = 0; pkey < IA2_MAX_COMPARTMENTS; pkey++) {
      if (start_addr == (*partition_alloc_thread_isolated_pool_base_address)[pkey]) {
        fprintf(log, "[heap:compartment %zu]", pkey);
        break;
      }
    }
  }
}

#else // IA2_DEBUG_MEMORY

static void label_memory_map(FILE *log, uintptr_t start_addr) {}

#endif // IA2_DEBUG_MEMORY

// `getline` calls `malloc` inside of `libc`,
// but we wrap `malloc` with `__wrap_malloc`,
// so we need to free what `getline` allocated with `__real_free`.
typeof(IA2_IGNORE(free)) __real_free;

void ia2_log_memory_maps(FILE *log) {
  FILE *maps = fopen("/proc/self/maps", "r");
  assert(maps);

  // Skip dev and inode.
  fprintf(log, "  start addr-end addr     perms offset  path\n");

  char *line = NULL;
  size_t line_cap = 0;
  while (true) {
    const ssize_t line_len = getline(&line, &line_cap, maps);
    if (line_len == -1) {
      break;
    }

    // Remove trailing newline.
    if (line_len > 0 && line[line_len - 1] == '\n') {
      line[line_len - 1] = 0;
    }

    // Parse `/proc/self/maps` line.
    uintptr_t start_addr = 0;
    uintptr_t end_addr = 0;
    char perms[4] = {0};
    uintptr_t offset = 0;
    unsigned int dev_major = 0;
    unsigned int dev_minor = 0;
    ino_t inode = 0;
    int path_index = 0;
    const int vars_matched = sscanf(line, "%lx-%lx %4c %lx %x:%x %lu %n", &start_addr, &end_addr, perms, &offset, &dev_major, &dev_minor, &inode, &path_index);
    if (vars_matched != 8) {
      fprintf(log, "%s\n", line);
      fprintf(stderr, "error parsing /proc/self/maps line: %s\n", line);
      continue;
    }
    const char *path = line + path_index;

    // Skip dev and inode.
    fprintf(log, "%08lx-%08lx %.4s %08lx ", start_addr, end_addr, perms, offset);

    const size_t path_len = (size_t)line_len - path_index - 1;
    if (path_len != 0) {
      fprintf(log, "%s", path);
    } else {
      // No path, try to identify it.
      label_memory_map(log, start_addr);
    }

    fprintf(log, "\n");
  }

  __real_free(line);
  fclose(maps);
}
