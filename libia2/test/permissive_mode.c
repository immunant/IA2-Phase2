#define _GNU_SOURCE
#include <ia2.h>
#include <permissive_mode.h>

INIT_RUNTIME(1);
INIT_COMPARTMENT(1);

int main(int argc, char** argv) {
    char* buffer = NULL;
    assert(ia2_get_pkru() == 0xFFFFFFF0);

    /* allocate an extra pkey */
    assert(pkey_alloc(0, PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE) == 2);

    buffer = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

    pkey_mprotect(buffer, 4096, PROT_READ | PROT_WRITE, 1);
    buffer[0] = 'a';

    pkey_mprotect(buffer, 4096, PROT_READ | PROT_WRITE, 2);
    buffer[0] = 'b';

    assert(ia2_get_pkru() == 0xFFFFFFF0);

    return 0;
}