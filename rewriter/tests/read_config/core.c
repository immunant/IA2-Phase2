/*
RUN: cat read_config_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ia2.h>
#include "builtin.h"
#include "core.h"

#define IA2_COMPARTMENT 3
#include <ia2_compartment_init.inc>

static void parse_array(char *opt, void *out);

static struct cfg_opt opts[3] IA2_SHARED_DATA = {
    {
        "name",
        str,
        // REWRITER: IA2_FN(parse_str),
        parse_str,
    },
    {
        "num_options",
        u32,
        // REWRITER: IA2_FN(parse_u32),
        parse_u32,
    },
    {
        "array",
        other,
        // REWRITER: IA2_FN(parse_array),
        parse_array,
    },
};

static void parse_array(char *opt, void *out) {
    uint8_t **res = out;
    memcpy(*res, opt, sizeof(uint8_t[3]));
}

// The arguments to the following functions point to the main binary so we don't
// need to use a shared buffer

// LINKARGS: --wrap=get_core_opt
struct cfg_opt *get_core_opt(char *name) {
    static bool registered = false;
    if (!registered) {
        register_plugin(0);
        registered = true;
    }
    for (size_t i = 0; i < 3; i++) {
        if (!strcmp(opts[i].name, name)) {
            return &opts[i];
        }
    }
    printf("Option %s not found!", name);
    exit(-1);
}

// LINKARGS: --wrap=print_array
void print_array(uint8_t ar[3]) {
    printf("[%x, %x, %x]\n", ar[0], ar[1], ar[2]);
}
