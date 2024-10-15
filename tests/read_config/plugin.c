/*
RUN: true
We need this because main.c uses LINKARGS checks but not this file.
*/

// Check that readelf shows exactly one executable segment
#include "plugin.h"
#include "core.h"
#include <ia2.h>
#include <ia2_test_runner.h>

#include <stdlib.h>
#include <string.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

// A custom parsing function for a type defined by the plugin
static void parse_tuple(char *opt, void *out);

// IA2_SHARED_DATA makes all values in this struct shared, which includes the
// `char *` but not the string literals. This means that the other compartment
// can see where the string literals are but will not be able to read them. This
// is fine since the other compartment doesn't need to read them.
static struct cfg_opt opts[6] IA2_SHARED_DATA = {
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
        "debug_mode",
        boolean,
        // REWRITER: IA2_FN(parse_bool),
        parse_bool,
    },
    {
        "magic_val",
        other,
        // REWRITER: IA2_FN(parse_tuple),
        parse_tuple,
    },
    {
        "some_flag",
        boolean,
        // REWRITER: IA2_FN(parse_bool),
        parse_bool,
    },
    {
        "random_seed",
        u32,
        // REWRITER: IA2_FN(parse_u32),
        parse_u32,
    },
};

// The plugin's parsing function `parse_tuple` needs access to this value in
// compartment 2.
const size_t tuple_size = sizeof(struct tuple);

static void parse_tuple(char *opt, void *out) {
  struct tuple **res = out;
  memcpy(*res, opt, tuple_size);
}

// Reading `name` is fine since the heap is shared
struct cfg_opt *get_opt(char *name) {
  for (size_t i = 0; i < 6; i++) {
    if (!strcmp(opts[i].name, name)) {
      // Returning a reference is fine since `opts` is shared and the main
      // compartment doesn't need to read the strings.
      return &opts[i];
    }
  }
  cr_log_info("Option %s not found!", name);
  exit(-1);
}

// LINKARGS: --wrap=print_tuple
void print_tuple(struct tuple *tup) {
  // These derefs are fine since the heap is shared.
  cr_log_info("(%x, %x)", tup->first, tup->second);
}
