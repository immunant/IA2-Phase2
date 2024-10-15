/*
RUN: cat heap_two_keys_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <ia2_test_runner.h>
#include <stdio.h>
#include <ia2.h>
#include "exported_fn.h"


#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

// LINKARGS: --wrap=read_from_plugin
uint8_t read_from_plugin(uint8_t *ptr) {
  if (ptr == NULL) {
    return -1;
  }
  uint8_t read = *ptr;
  return read;
}

// LINKARGS: --wrap=read_from_plugin_expect_fault
uint8_t read_from_plugin_expect_fault(uint8_t *ptr) {
  if (ptr == NULL) {
    return -1;
  }
  uint8_t read = CHECK_VIOLATION(*ptr);
  return read;
}

// LINKARGS: --wrap=trigger_compartment_init
void trigger_compartment_init(void) {}

// LINKARGS: --wrap=write_from_plugin
void write_from_plugin(uint8_t *ptr, uint8_t value) {
  if (ptr == NULL) {
    return;
  }
  *ptr = value;
}

// LINKARGS: --wrap=write_from_plugin_expect_fault
void write_from_plugin_expect_fault(uint8_t *ptr, uint8_t value) {
  if (ptr == NULL) {
    return;
  }
  CHECK_VIOLATION(*ptr = value);
}
