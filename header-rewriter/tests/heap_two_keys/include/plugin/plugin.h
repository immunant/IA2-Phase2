#pragma once
#include <stdint.h>
#include <stddef.h>

void trigger_compartment_init(void);

#define DECLARE_FUNCTIONS(ty)                          \
    ty read_##ty(ty *ptr);                             \
    ty read_##ty##_expect_fault(ty *ptr);              \
    void write_##ty(ty *ptr, ty value);                \
    void write_##ty##_expect_fault(ty *ptr, ty value)

DECLARE_FUNCTIONS(uint8_t);
DECLARE_FUNCTIONS(uint16_t);
DECLARE_FUNCTIONS(uint32_t);
DECLARE_FUNCTIONS(uint64_t);
