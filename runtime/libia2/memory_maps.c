#include "memory_maps.h"
#include "ia2.h"

#include <stdatomic.h>

// Only enable this code that stores these addresses when debug logging is enabled.
// This reduces the trusted codebase and avoids runtime overhead.
#if IA2_DEBUG_MEMORY

// It's much simpler to only support a static number of created threads,
// especially because we want to have very few dependencies.
// If a program needs more threads, you can just increase this number.
#define MAX_THREADS 512

struct ia2_all_threads_metadata {
  /// This is the number of threads registered,
  /// and it is monotonically increasing by 1.
  ///
  /// It may be transiently higher than `MAX_THREADS`,
  /// but will abort if that happens (other threads may be observe a higher value).
  _Atomic size_t num_threads;
  pid_t tids[MAX_THREADS];

  /// Should be initialized to 0.
  struct ia2_thread_metadata thread_metadata[MAX_THREADS];
};

#define min(a, b) ((a) < (b) ? (a) : (b))

// All zeroed, so this should go in `.bss` and only have pages lazily allocated.
static struct ia2_all_threads_metadata IA2_SHARED_DATA threads = {
    .num_threads = 0,
    .thread_metadata = {0},
};

struct ia2_thread_metadata *ia2_all_threads_metadata_new_for_current_thread(struct ia2_all_threads_metadata *const this) {
  const size_t thread = atomic_fetch_add(&this->num_threads, 1);
  if (thread >= MAX_THREADS) {
    fprintf(stderr, "created %zu threads, but can't store them all (max is MAX_THREADS: %zu)\n",
            thread + 1, (size_t)MAX_THREADS);
    abort();
  }

  const pid_t tid = gettid();
  this->tids[thread] = tid;
#if IA2_VERBOSE
  fprintf(stderr, "new thread metadata for thread %ld\n", (long)tid);
#endif

  struct ia2_thread_metadata *metadata = &this->thread_metadata[thread];
  metadata->tid = tid;
  metadata->thread = pthread_self();
  return metadata;
}

struct ia2_thread_metadata *ia2_all_threads_metadata_get_for_current_thread(struct ia2_all_threads_metadata *const this) {
  const pid_t tid = gettid();

  // We won't see threads created/registered after this,
  // but `ia2_all_threads_metadata_new_for_current_thread`
  // was supposed to be called first for this function to find it.
  const size_t num_threads = min(MAX_THREADS, atomic_load(&this->num_threads));

#if IA2_VERBOSE
  fprintf(stderr, "get thread metadata for thread %ld\n", (long)tid);
  fprintf(stderr, "searching through %zu threads\n", num_threads);
  fprintf(stderr, "&this->num_threads = %p\n", &this->num_threads);
  fprintf(stderr, "this = %p\n", this);
  fprintf(stderr, "&threads = %p\n", &threads);
  fprintf(stderr, "getpid() = %ld\n", (long)getpid());
#endif

  struct ia2_thread_metadata *metadata = NULL;
  for (size_t thread = 0; thread < num_threads; thread++) {
    if (this->tids[thread] == tid) {
      return &this->thread_metadata[thread];
    }
  }

  fprintf(stderr,
          "ia2_thread_metadata not found for thread %ld\n"
          "ia2_thread_metadata_new_for_current_thread must not have been previously called on this thread\n",
          (long)tid);
  abort();
}

struct ia2_addr_location ia2_all_threads_metadata_find_addr(struct ia2_all_threads_metadata *const this, const uintptr_t addr) {
  // We won't see threads created/registered after this,
  // but this is supposed to be best effort, so that's okay.
  const size_t num_threads = min(MAX_THREADS, atomic_load(&this->num_threads));

  for (size_t thread = 0; thread < this->num_threads; thread++) {
    const pid_t tid = this->tids[thread];
    const struct ia2_thread_metadata *const thread_metadata = &this->thread_metadata[thread];

    if (addr == thread_metadata->tls_addr_compartment1_first || addr == thread_metadata->tls_addr_compartment1_second) {
      return (struct ia2_addr_location){
          .name = "tls",
          .thread_metadata = thread_metadata,
          .compartment = 1,
      };
    }

    for (int compartment = 0; compartment < IA2_MAX_COMPARTMENTS; compartment++) {
      if (addr == thread_metadata->stack_addrs[compartment]) {
        return (struct ia2_addr_location){
            .name = "stack",
            .thread_metadata = thread_metadata,
            .compartment = compartment,
        };
      }
      if (addr == thread_metadata->tls_addrs[compartment]) {
        return (struct ia2_addr_location){
            .name = "tls",
            .thread_metadata = thread_metadata,
            .compartment = compartment,
        };
      }
    }
  }

  return (struct ia2_addr_location){
      .name = NULL,
      .thread_metadata = NULL,
      .compartment = -1,
  };
}

struct ia2_thread_metadata *ia2_thread_metadata_new_for_current_thread(void) {
  return ia2_all_threads_metadata_new_for_current_thread(&threads);
}

// /// Register the main thread's `ia2_thread_metadata`.
// /// Non-main threads are registered upon thread creation.
// void __attribute__((constructor)) register_main_thread_metadata(void) {
//   ia2_thread_metadata_new_for_current_thread();
// }

void setup_thread_metadata(void) {
  ia2_thread_metadata_new_for_current_thread();
}

struct ia2_thread_metadata *ia2_thread_metadata_get_for_current_thread(void) {
  return ia2_all_threads_metadata_get_for_current_thread(&threads);
}

struct ia2_addr_location ia2_addr_location_find(const uintptr_t addr) {
  return ia2_all_threads_metadata_find_addr(&threads, addr);
}

extern uintptr_t (*partition_alloc_thread_isolated_pool_base_address)[IA2_MAX_COMPARTMENTS];

static void label_memory_map(FILE *log, uintptr_t start_addr) {
  const struct ia2_addr_location location = ia2_addr_location_find(start_addr);
  const struct ia2_thread_metadata *metadata = location.thread_metadata;

  if (location.name) {
    char thread_name[16] = {0};
    const bool has_thread_name = pthread_getname_np(metadata->thread, thread_name, sizeof(thread_name)) == 0;

    Dl_info dl_info = {0};
    const bool has_dl_info = dladdr((void *)metadata->start_fn, &dl_info);

    fprintf(log, "[%s:compartment %d:tid %ld", location.name, location.compartment, (long)metadata->tid);
    if (has_thread_name) {
      fprintf(log, " (thread %s)", thread_name);
    }
    fprintf(log, " (start fn ");
    if (!metadata->start_fn) {
      // `metadata->start_fn` is always set during `__wrap_pthread_create`/`ia2_thread_begin`,
      // so if it wasn't set, then it must be the main thread, started in `main`.
      fprintf(log, "main");
    } else if (has_dl_info && dl_info.dli_sname) {
      fprintf(log, "%s", dl_info.dli_sname);
    } else {
      fprintf(log, "%p", metadata->start_fn);
    }
    fprintf(log, ")]");
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

void setup_thread_metadata(void) {}

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
    const int expected_vars_matched = 7; // Note that "%n" doesn't count as a matched var.
    if (vars_matched != expected_vars_matched) {
      fprintf(log, "%s\n", line);
      fprintf(stderr, "error parsing /proc/self/maps line (matched %d vars instead of %d): %s\n",
              vars_matched, expected_vars_matched, line);
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
