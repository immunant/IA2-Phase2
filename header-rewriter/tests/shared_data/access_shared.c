#include <stdio.h>
#include "access_shared.h"

static uint8_t previous = 255;

void read_shared(uint8_t *shared) {
    if (shared) {
        printf("read %d from shared variable\n", *shared);
    }
}

uint8_t write_shared(uint8_t *shared) {
    uint8_t new_val = previous;
    if (shared) {
        previous = *shared;
        printf("writing %d to shared variable\n", new_val);
        *shared = new_val;
    }
    return new_val;
}
