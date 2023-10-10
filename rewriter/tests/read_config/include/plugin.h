#pragma once
#include "builtin.h"

struct tuple {
    uint32_t first;
    uint32_t second;
};

// Get the config option for a given entry name.
struct cfg_opt *get_opt(char *name);

void print_tuple(struct tuple *);
