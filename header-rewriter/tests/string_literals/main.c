#include "plugin.h"
#include <ia2.h>
#include <stdio.h>
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

// This test uses two protection keys
INIT_RUNTIME(2);
INIT_COMPARTMENT(1);

IA2_SHARED_STR(shared_str, "this is the main binary's shared string\\n");
const char *secret_str = "this is the main binary's secret string\n";

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Run with `plugin` or `main` as the first argument\n");
    return -1;
  }

  if (!strcmp(argv[1], "plugin")) {
    LOG("checking if the plugin secret string is safe");
    // Check that we can read the plugin's shared string
    LOG("%s", get_plugin_shared_str());
    CHECK_VIOLATION(LOG("%s", get_plugin_secret_str()));
  } else if (!strcmp(argv[1], "main")) {
    LOG("checking if the main secret string is safe");
    read_main_strings(&shared_str, secret_str);
  } else {
    printf("Invalid argument\n");
    return -1;
  }
}
