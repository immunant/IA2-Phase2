#include <stdio.h>
#include <unistd.h>
#include <ia2.h>
#include "plugin.h"

INIT_RUNTIME(1);
INIT_COMPARTMENT(0);

uint32_t secret = 0x09431233;

void print_message(void) {
    printf("%s: this is defined in the main binary\n", __func__);
}

int main(int argc, char **argv) {
    start_plugin();
}
