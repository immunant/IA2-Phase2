#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include "exported_fn.h"
#include "test_fault_handler.h"

void start_plugin(void) {
    cr_log_info("%s: this is defined in the plugin\n", __func__);
    print_message();
    if (!clean_exit) {
        cr_log_info("%s: the secret is %d\n", __func__, CHECK_VIOLATION(secret));
        cr_fatal("Should have segfaulted on cross-boundary access");
    }
}
