#include <ia2.h>
#include <ia2_memory_maps.h>
#include <ia2_test_runner.h>

#include <pthread.h>

INIT_RUNTIME(4);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
  ia2_register_compartment("lib2.so", 2, NULL);
  ia2_register_compartment("lib3.so", 3, NULL);
  ia2_register_compartment("lib4.so", 4, NULL);
}

Test(memory_maps, alloc) {
  // Alloc and free so that partition-alloc allocates its compartment heaps.
  free(malloc(1024));

  ia2_log_memory_maps(stdout);
}

__thread int tls;

void *start_thread(void *const arg) {
  // Mostly bogus stuff.
  // Just want to use stack, heap, and TLS.
  int a[4096] = {0};
  malloc(10);

  // We want to inspect the labeled memory map with all of the threads,
  // so if we joined them first then we wouldn't be able to see them.
  pause();
  return (void *)((void *)a - (void *)&tls);
}

// `open_memstream` calls `malloc` inside of `libc`,
// but we wrap `malloc` with `__wrap_malloc`,
// so we need to free what `getline` allocated with `__real_free`.
typeof(IA2_IGNORE(free)) __real_free;

Test(memory_maps, multithreaded) {
  pthread_t threads[10];
  for (size_t i = 0; i < 10; i++) {
#if IA2_ENABLE
    pthread_create(&threads[i], NULL, start_thread, NULL);
#endif
  }

  // Name some (but not all) of them to see how we print them.
  pthread_setname_np(threads[2], "two");
  pthread_setname_np(threads[3], "three");
  pthread_setname_np(threads[5], "five");
  pthread_setname_np(threads[7], "seven");

  // Don't write to stdout/stderr directly,
  // since it interleaves with other concurrent output.
  char *buf = NULL;
  size_t buf_len = 0;
  FILE *const log = open_memstream(&buf, &buf_len);

  // Log before joining threads.
  // We want to inspect the labeled memory map with all of the threads,
  // so if we joined them first then we wouldn't be able to see them.
  ia2_log_memory_maps(log);

  fclose(log);
  fflush(stdout);
  write(STDOUT_FILENO, buf, buf_len);
  __real_free(buf);
}
