/*
RUN: cat function_allowlist_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include "library.h"
#include <stdio.h>

// LINKARGS: --wrap=foo
void foo() { printf("foo\n"); }
