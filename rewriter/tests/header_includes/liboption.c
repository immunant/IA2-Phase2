/*
*/
#include <criterion/logging.h>
#include "liboption.h"
#include "types.h"

Option None() {
    cr_log_info("returning `None`");
    Option none = {
        .value = 0,
        .present = false,
    };
    return none;
}

Option Some(int x) {
    cr_log_info("returning `Some(%d)`", x);
    Option opt = {
        .value = x,
        .present = true,
    };
    return opt;
}
