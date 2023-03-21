/*
RUN: cat two_shared_ranges_call_gates_2.ld | FileCheck --check-prefix=LINKARGS %s
RUN: %binary_dir/tests/two_shared_ranges/two_shared_ranges_main_wrapped plugin | diff %binary_dir/tests/two_shared_ranges/plugin.out -
RUN: %binary_dir/tests/two_shared_ranges/two_shared_ranges_main_wrapped main | diff %binary_dir/tests/two_shared_ranges/main.out -
TODO: %binary_dir/tests/two_shared_ranges/two_shared_rangesr_main_wrapped clean_exit | diff %source_dir/tests/two_shared_ranges/Output/clean_exit.out -
RUN: readelf -lW %binary_dir/tests/two_shared_ranges/two_shared_ranges_main_wrapped | FileCheck --check-prefix=SEGMENTS %s
*/

// Check that readelf shows exactly one executable segment
// SEGMENTS-COUNT-1: LOAD{{.*}}R E
// SEGMENTS-NOT:     LOAD{{.*}}R E

#include <stdio.h>
#include <unistd.h>
#include <ia2.h>
#include "plugin.h"
#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

// This test uses two protection keys
INIT_RUNTIME(2);
INIT_COMPARTMENT(1);

uint32_t secret = 0x09431233;
uint32_t shared IA2_SHARED_DATA = 0xb75784ee;

static bool steal_plugin_secret = false;
// Running in debug mode prints the addresses of the secrets defined in each
// compartment. This is off by default to simplify the diff of stdout against
// the expected output.
bool debug_mode IA2_SHARED_DATA = false;

bool clean_exit IA2_SHARED_DATA = false;

// LINKARGS: --wrap=print_message
void print_message(void) {
    LOG("this is defined in the main binary");
    if (debug_mode) {
        LOG("the main secret is at %p", &secret);
        LOG("the plugin shared data is at %p", &plugin_shared);
    }
    LOG("the main secret is %x", secret);
    LOG("the plugin shared data is %x", plugin_shared);
    if (steal_plugin_secret) {
        LOG("the plugin secret is %x\n", CHECK_VIOLATION(plugin_secret));
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Run with `plugin`, `main` or `clean_exit` as the first argument\n");
        return -1;
    }
    if (!strcmp(argv[1], "plugin")) {
        LOG("checking if the plugin secret is safe");
        steal_plugin_secret = true;
    } else if (!strcmp(argv[1], "main")) {
        LOG("checking if the main secret is safe");
    } else {
        LOG("checking if the program can exit cleanly");
        clean_exit = true;
    }
    if (argc == 3 && !strcmp(argv[2], "debug")) {
        debug_mode = true;
    }
    start_plugin();
}
