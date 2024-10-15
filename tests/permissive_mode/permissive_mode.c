#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ia2_test_runner.h>
#include <ia2.h>
#include <permissive_mode.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

Test(permissive_mode, main) {
    char* buffer = NULL;
    cr_assert(ia2_get_pkru() == 0xFFFFFFF0);

    /* allocate an extra pkey */
    cr_assert(pkey_alloc(0, PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE) == 2);

    buffer = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

    pkey_mprotect(buffer, 4096, PROT_READ | PROT_WRITE, 1);
    buffer[0] = 'a';

    pkey_mprotect(buffer, 4096, PROT_READ | PROT_WRITE, 2);
    buffer[0] = 'b';

    cr_assert(ia2_get_pkru() == 0xFFFFFFF0);
}
