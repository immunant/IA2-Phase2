#include <stdio.h>
#include <unistd.h>
#include <ia2.h>
#include "plugin.h"

INIT_COMPARTMENT;

uint32_t secret = 0x09431233;

void print_message(void) {
    printf("%s: this is defined in the main binary\n", __func__);
    printf("%s: the secret is %x\n", __func__, secret);
}

int main(int argc, char **argv) {
    start_plugin();
}
