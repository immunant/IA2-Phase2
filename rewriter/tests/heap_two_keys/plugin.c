/*
RUN: cat heap_two_keys_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <stdio.h>
#include <ia2.h>
#include "exported_fn.h"
#include "test_fault_handler.h"

INIT_COMPARTMENT(2);

// LINKARGS: --wrap=trigger_compartment_init
void trigger_compartment_init(void) {}

#define DEFINE_FUNCTIONS(ty)                            \
    ty read_##ty(ty *ptr) {                             \
        if (ptr == NULL) {                              \
            return -1;                                  \
        }                                               \
        ty read = *ptr;                                 \
        return read;                                    \
    }                                                   \
    ty read_##ty##_expect_fault(ty *ptr) {              \
        if (ptr == NULL) {                              \
            return -1;                                  \
        }                                               \
        ty read = CHECK_VIOLATION(*ptr);                \
        return read;                                    \
    }                                                   \
    void write_##ty(ty *ptr, ty value) {                \
        if (ptr == NULL) {                              \
            return;                                     \
        }                                               \
        *ptr = value;                                   \
    }                                                   \
    void write_##ty##_expect_fault(ty *ptr, ty value) { \
        if (ptr == NULL) {                              \
            return;                                     \
        }                                               \
        CHECK_VIOLATION(*ptr = value);                  \
        return;                                         \
    }

DEFINE_FUNCTIONS(uint8_t);
DEFINE_FUNCTIONS(uint16_t);
DEFINE_FUNCTIONS(uint32_t);
DEFINE_FUNCTIONS(uint64_t);
