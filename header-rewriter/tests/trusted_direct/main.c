#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ia2.h>
#include "plugin.h"

// This test checks that an untrusted library can call a trusted main binary. An
// MPK violation is triggered from the untrusted library if no arguments are
// passed in. Otherwise the program exits cleanly.

INIT_RUNTIME(1);
INIT_COMPARTMENT(1);

uint32_t secret = 0x09431233;

bool clean_exit IA2_SHARED_DATA = false;

void print_message(void) {
    printf("%s: the secret 0x%lx is defined in the main binary\n", __func__, secret);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        clean_exit = true;
    }
    start_plugin();
}
