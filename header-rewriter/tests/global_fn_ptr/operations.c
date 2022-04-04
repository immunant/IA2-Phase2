#include "operations.h"
#include <stdio.h>

extern Op operations[2];

void call_operations(void) {
    for (int i = 0; i < 2; i++) {
        // TODO: Add a way to share strings between compartments
        //printf("%s\n", operations[i].desc.data);
        uint32_t x = 18923;
        uint32_t y = 24389;
        uint32_t res = operations[i].function(x, y);
        printf("%d = f(%d, %d)\n", res, x, y);
    }
}
