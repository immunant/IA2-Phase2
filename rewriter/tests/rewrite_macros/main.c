/*
RUN: cat main.c | FileCheck --match-full-lines --check-prefix=REWRITER %s
*/
#include "lib.h"
#include <stddef.h>
#include <ia2.h>
#include <criterion/criterion.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

Test(rewrite_macros, main) {
    init_actions();

    // The next two lines use macros that cannot be easily rewritten so the
    // rewriter should leave them unmodified.
    if (check_actions(NULL)) {}
    call_add_event(NULL);

    struct event *evt = get_event();
    // Test that the FnPtrCall pass can rewrite simple macros
    // REWRITER: IA2_CALL(add_event, 0)(evt);
    add_event(evt);
    // REWRITER: IA2_CALL(actions.add, 0)(evt);
    actions.add(evt);

    bool(*fn)(struct event *evt) = add_event;
    bool(*fn2)(struct event *evt) = actions.add;

    // Test that the FnPtrEq pass can rewrite simple macros
    // REWRITER: if (IA2_ADDR(fn) == IA2_ADDR(add_event)) {}
    if (fn == add_event) {}
    // REWRITER: if (IA2_ADDR(fn) == IA2_ADDR(actions.add)) {}
    if (fn == actions.add) {}

    // REWRITER: IA2_CALL(fn, 0)(evt);
    fn(evt);
}
