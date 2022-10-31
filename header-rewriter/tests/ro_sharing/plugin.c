
#define IA2_INIT_COMPARTMENT 2
#include <ia2.h>

#include "test_fault_handler.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// All string literals should be in .rodata
const char *plugin_str = "this is the plugin's string\n";

// Global in .rodata
const uint32_t plugin_shared_ro = 0x730283;

// Global in .data
uint32_t plugin_secret_rw = 0x8294671;

void read_main_string(const char *str) {
  // Check that we can read a string passed from main
  LOG("%s", str);
}

void read_main_uint(const uint32_t *shared, const uint32_t *secret) {
  // Check that we can read a pointer to rodata passed from main
  LOG("0x%x", *shared);

  // Check that we can't read a pointer to data passed from main
  CHECK_VIOLATION(LOG("0x%x", *secret));
}

const char *get_plugin_str() {
  return plugin_str;
}

const uint32_t *get_plugin_uint(bool secret) {
  if (secret)
    return &plugin_secret_rw;
  else
    return &plugin_shared_ro;
}
