#pragma once
#include "builtin.h"
#include <stdint.h>
#include <stdbool.h>

struct cfg_opt *get_core_opt(char *name);
void print_array(uint8_t ar[3]);
