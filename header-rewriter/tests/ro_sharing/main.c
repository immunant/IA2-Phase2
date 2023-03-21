/*
RUN: sh -c 'if [ ! -s "ro_sharing_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
RUN: sh -c 'if [ ! -s "ro_sharing_call_gates_2.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
RUN: %binary_dir/tests/ro_sharing/ro_sharing_main_wrapped plugin | diff %binary_dir/tests/ro_sharing/plugin.out -
RUN: %binary_dir/tests/ro_sharing/ro_sharing_main_wrapped main | diff %binary_dir/tests/ro_sharing/main.out -
*/
#include "plugin.h"
#include <ia2.h>
#include <stdio.h>
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

// This test checks that all RO data mapped in from executable files is shared.
// This is needed so that the dynamic linker can read ELF metadata. Read-only
// data mapped in from the executable file should not be secret (files on disk
// are not assumed to be secret in our model).

// This test uses two protection keys
INIT_RUNTIME(2);
INIT_COMPARTMENT(1);

// All string literals should be in .rodata
const char *main_str = "this is the main binary's string\n";

// Global in .rodata
const uint32_t main_shared_ro = 0x09431233;

// Global in .data
uint32_t main_secret_rw = 0x88997766;

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Run with `plugin` or `main` as the first argument\n");
    return -1;
  }

  if (!strcmp(argv[1], "plugin")) {
    LOG("%s", get_plugin_str());
    LOG("0x%x", *get_plugin_uint(false));
    CHECK_VIOLATION(LOG("0x%x", *get_plugin_uint(true)));
  } else if (!strcmp(argv[1], "main")) {
    read_main_string(main_str);
    read_main_uint(&main_shared_ro, &main_secret_rw);
  } else {
    printf("Invalid argument\n");
    return -1;
  }
}
