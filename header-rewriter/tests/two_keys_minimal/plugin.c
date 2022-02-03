#include <stdio.h>
#include <ia2.h>
#include "exported_fn.h"
#include "test_fault_handler.h"

INIT_COMPARTMENT_N(1);

void start_plugin(void) {
    printf("%s: this is defined in the plugin\n", __func__);
    print_message();
    printf("%s: the secret is %x\n", __func__, CHECK_VIOLATION(secret));
}
