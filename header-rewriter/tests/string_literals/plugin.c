#include "test_fault_handler.h"
#include <ia2.h>
#include <stdio.h>
#include <string.h>

INIT_COMPARTMENT(2);

IA2_SHARED_STR(plugin_shared_str, "this is the plugin's shared string\\n");
const char *plugin_secret_str = "this is the plugin's secret string\n";

extern const char *secret_str;

void read_main_strings(const char *shared, const char *secret) {
  // Check that we can read the shared string
  LOG("%s", shared);
  CHECK_VIOLATION(LOG("%s", secret));
}

const char *get_plugin_shared_str() {
    return &plugin_shared_str;
}

const char *get_plugin_secret_str() {
    return plugin_secret_str;
}
