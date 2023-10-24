/*
*/
#include <stdio.h>
#include <ia2.h>
#include "exported_fn.h"
#include "test_fault_handler.h"

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

uint8_t read_from_plugin(uint8_t *ptr) {
  if (ptr == NULL) {
    return -1;
  }
  uint8_t read = *ptr;
  return read;
}

uint8_t read_from_plugin_expect_fault(uint8_t *ptr) {
  if (ptr == NULL) {
    return -1;
  }
  uint8_t read = CHECK_VIOLATION(*ptr);
  return read;
}

void trigger_compartment_init(void) {}

void write_from_plugin(uint8_t *ptr, uint8_t value) {
  if (ptr == NULL) {
    return;
  }
  *ptr = value;
}

void write_from_plugin_expect_fault(uint8_t *ptr, uint8_t value) {
  if (ptr == NULL) {
    return;
  }
  CHECK_VIOLATION(*ptr = value);
}
