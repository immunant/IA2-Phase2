/*
*/
#include "types.h"

int unwrap_or(Option opt, int default_value) {
    if (opt.present) {
        return opt.value;
    } else {
        return default_value;
    }
}
