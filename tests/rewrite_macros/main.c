/*
RUN: cat main.c | FileCheck --match-full-lines --check-prefix=REWRITER %s
*/
#include "lib.h"
#include <stddef.h>
#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
}

Test(rewrite_macros, main) {
    init_actions();

    // The next two lines use macros that cannot be easily rewritten so the
    // rewriter should leave them unmodified.
    if (check_actions(NULL)) {}
    call_add_event(NULL);

    struct event *evt = get_event();
    // Test that the FnPtrCall pass can rewrite simple macros
    // REWRITER: IA2_CALL(add_event, _ZTSPFbP5eventE, evt);
    add_event(evt);
    // REWRITER: IA2_CALL(actions.add, _ZTSPFbP5eventE, evt);
    actions.add(evt);

    bool(*fn)(struct event *evt) = add_event;
    bool(*fn2)(struct event *evt) = actions.add;

    // Test that the FnPtrEq pass can rewrite simple macros
    // REWRITER: if (IA2_ADDR(fn) == IA2_ADDR(add_event)) {}
    if (fn == add_event) {}
    // REWRITER: if (IA2_ADDR(fn) == IA2_ADDR(actions.add)) {}
    if (fn == actions.add) {}

    // REWRITER: IA2_CALL(fn, _ZTSPFbP5eventE, evt);
    fn(evt);
}
