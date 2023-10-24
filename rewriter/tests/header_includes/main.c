#include "liboption.h"
#include "types.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <ia2.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

Test(header_includes, main) {
    Option x = Some(3);
    Option none = None();
    cr_assert_eq(unwrap_or(x, -1), 3);
    cr_assert_eq(unwrap_or(none, -1), -1);
}
