/*
*/
#include <criterion/logging.h>
#include <stdlib.h>
#include <string.h>
#include <ia2.h>
// This is a hack. See includes in main.c.
#include "plugin.h"
#include "core.h"

static void parse_array(char *opt, void *out);

static struct cfg_opt opts[3] = {
    {
        "name",
        str,
        parse_str,
    },
    {
        "num_options",
        u32,
        parse_u32,
    },
    {
        "array",
        other,
        parse_array,
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
    cr_log_info("Option %s not found!", name);
    exit(-1);
}

void print_array(uint8_t ar[3]) {
    cr_log_info("[%x, %x, %x]", ar[0], ar[1], ar[2]);
}
