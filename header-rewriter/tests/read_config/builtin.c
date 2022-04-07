#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ia2.h>
// This is a hack. See includes in main.c.
#include "plugin.h"
#include "core.h"

static void parse_array(char *opt, void *out);

// The indirect callsite in main.c drops the target function to pkey 0, but
// `parse_str` calls strcpy through the main binary's PLT. This means we need to
// wrap parse_str to ensure it's called with pkey 1. The same goes for parse_u32
// and parse_array.
IA2_DEFINE_WRAPPER(parse_str, _ZTSPFvPcPvE, 1);
IA2_DEFINE_WRAPPER(parse_u32, _ZTSPFvPcPvE, 1);
IA2_DEFINE_WRAPPER(parse_array, _ZTSPFvPcPvE, 1);

static struct cfg_opt opts[3] = {
    {
        "name",
        str,
        IA2_WRAPPER(parse_str, 1),
    },
    {
        "num_options",
        u32,
        IA2_WRAPPER(parse_u32, 1),
    },
    {
        "array",
        other,
        IA2_WRAPPER(parse_array, 1),
    },
};

static void parse_array(char *opt, void *out) {
    uint8_t **res = out;
    memcpy(*res, opt, sizeof(uint8_t[3]));
}

// The arguments to the following functions point to the main binary so we don't
// need to use a shared buffer

struct cfg_opt *get_builtin_opt(char *name) {
    for (size_t i = 0; i < 3; i++) {
        if (!strcmp(opts[i].name, name)) {
            return &opts[i];
        }
    }
    printf("Option %s not found!", name);
    exit(-1);
}

void print_array(uint8_t ar[3]) {
    printf("[%x, %x, %x]\n", ar[0], ar[1], ar[2]);
}
