#pragma once

#include <stdbool.h>

extern int lib_secret;

void test_handler_from_lib(void);
void install_sighandler_in_lib(bool rewrite);