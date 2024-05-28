#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ia2.h>
#include <ia2_test_runner.h>
#include <permissive_mode.h>

#include <pthread.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

Test(permissive_mode, main) {
    char* buffer = NULL;
    cr_assert(ia2_get_tag() == 0xFFFFFFF0);

    /* allocate an extra pkey */
    cr_assert(pkey_alloc(0, PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE) == 2);

    buffer = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

    pkey_mprotect(buffer, 4096, PROT_READ | PROT_WRITE, 1);
    buffer[0] = 'a';

    pkey_mprotect(buffer, 4096, PROT_READ | PROT_WRITE, 2);
    buffer[0] = 'b';

    cr_assert(ia2_get_tag() == 0xFFFFFFF0);
}

Test(permissive_mode, alloc) {
    // Alloc and free so that partition-alloc allocates its compartment heaps.
    free(malloc(1024));
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

Test(permissive_mode, multithreaded) {
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

  // Exit before joining threads.
  // We want to inspect the labeled memory map with all of the threads,
  // so if we joined them first then we wouldn't be able to see them.
  exit(0);
}
