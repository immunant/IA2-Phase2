#ifndef ACCESS_SHARED_H
#define ACCESS_SHARED_H
#include <stdint.h>

void read_shared(uint8_t *shared);

uint8_t write_shared(uint8_t *shared, uint8_t new_value);

#endif
