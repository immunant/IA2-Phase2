/*
RUN: cat trusted_direct_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <stdio.h>
#include "exported_fn.h"
#include "test_fault_handler.h"

// LINKARGS: --wrap=start_plugin
void start_plugin(void) {
    printf("%s: this is defined in the plugin\n", __func__);
    print_message();
    if (!clean_exit) {
        printf("%s: the secret is %d\n", __func__, CHECK_VIOLATION(secret));
    }
}
