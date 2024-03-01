/*
RUN: cat should_segfault_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <criterion/criterion.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "print_secret.h"
#include "test_fault_handler.h"

static bool early_fault = false;

// Trigger a fault earlier than expected to test that CHECK_VIOLATION prints a
// different message than in the mpk violation case.
// LINKARGS: --wrap=do_early_fault
void do_early_fault() {
    early_fault = true;
}

// LINKARGS: --wrap=print_secret
void print_secret() {
    if (early_fault) {
        raise(SIGSEGV);
    }
    cr_assert(CHECK_VIOLATION(secret));
}
