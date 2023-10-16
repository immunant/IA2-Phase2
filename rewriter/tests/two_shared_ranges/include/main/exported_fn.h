#pragma once
#include <stdint.h>
#include <stdbool.h>

void print_message(void);

// This is exported to avoid an implicit decl error when the plugin tries to
// access it, but it's explicitly not shared to test that an MPK violation
// occurs.
extern uint32_t secret;

extern uint32_t shared;
