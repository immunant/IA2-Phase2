#pragma once
#include <stddef.h>
#include <stdint.h>

void trigger_compartment_init(void);

uint8_t read_from_plugin(uint8_t *ptr);
uint8_t read_from_plugin_expect_fault(uint8_t *ptr);
void write_from_plugin(uint8_t *ptr, uint8_t value);
void write_from_plugin_expect_fault(uint8_t *ptr, uint8_t value);
