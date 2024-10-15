/*
RUN: sh -c 'if [ ! -s "ro_sharing_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/
#include <ia2_test_runner.h>

#include <plugin.h>
#include <ia2.h>
#include <stdio.h>



// This test checks that all RO data mapped in from executable files is shared.
// This is needed so that the dynamic linker can read ELF metadata. Read-only
// data mapped in from the executable file should not be secret (files on disk
// are not assumed to be secret in our model).

// This test uses two protection keys
INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

// All string literals should be in .rodata
const char *main_str = "this is the main binary's string\n";

// Global in .rodata
const uint32_t main_shared_ro = 0x09431233;

// Global in .data
uint32_t main_secret_rw = 0x88997766;

Test(ro_sharing, main) {
  read_main_string(main_str);
  read_main_uint(&main_shared_ro, &main_secret_rw);
}

Test(ro_sharing, plugin) {
  cr_log_info("%s", get_plugin_str());
  cr_log_info("0x%x", *get_plugin_uint(false));
  cr_log_info("0x%x", CHECK_VIOLATION(*get_plugin_uint(true)));
}