#define IA2_INIT_COMPARTMENT 1
#include <ia2.h>

#include <stdint.h>
#include <assert.h>
#include "access_shared.h"

INIT_RUNTIME(1);

uint8_t shared_val[4097] IA2_SHARED_DATA = { 0 };

void check_shared_access(uint8_t *shared) {
    uint8_t original = *shared;
    read_shared(shared);
    assert(original == *shared);
    uint8_t old_val = write_shared(shared, original + 1);
    assert(original == old_val);
}

int main() {
    shared_val[0] = 23;
    shared_val[4096] = 254;
    check_shared_access(&shared_val[0]);
    check_shared_access(&shared_val[4096]);
}
