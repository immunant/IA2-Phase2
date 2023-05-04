// No need to execute the program here since the header exported by the main
// binary does that.
#pragma once
#include <stdint.h>

void start_plugin(void);

// This is exported to avoid an implicit decl error when the main binary tries
// to access it, but it's explicitly not shared to test that an MPK violation
// occurs.
extern uint32_t plugin_secret;

extern uint32_t plugin_shared;
