
#include "minimal.h"
#include <criterion/logging.h>

void arg1(int x) {
    cr_log_info("arg1");
}

void foo() {
    cr_log_info("foo");
}

int return_val() {
    cr_log_info("return_val");
    return 1;
}

