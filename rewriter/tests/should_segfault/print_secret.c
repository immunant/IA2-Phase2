#include <criterion/criterion.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "print_secret.h"
#include "test_fault_handler.h"

static bool early_fault = false;

// Trigger a fault earlier than expected to test that CHECK_VIOLATION prints a
// different message than in the mpk violation case.
void do_early_fault() {
    early_fault = true;
}

void print_secret() {
    if (early_fault) {
        raise(SIGSEGV);
    }
    cr_assert(CHECK_VIOLATION(secret));
}
