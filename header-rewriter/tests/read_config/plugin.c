#define IA2_INIT_COMPARTMENT 2
#include <ia2.h>

#include "core.h"
#include "plugin.h"
#include <stdio.h>
#include <string.h>

// A custom parsing function for a type defined by the plugin
static void parse_tuple(char *opt, void *out);

// The indirect callsite in main.c drops the target function to pkey 0, but
// parse_tuple needs access to this compartment (memcpy is called through the
// PLT) so must wrap it.
IA2_DEFINE_WRAPPER(parse_tuple, _ZTSPFvPcPvE, 2);

// In this source file, references to parse_str actually point to the wrapper
// __ia2_parse_str defined in the shim library. __ia2_parse_str expects a caller
// with pkey 2 (plugin) and calls the real parse_str with pkey 1 (the main
// binary's). The indirect calls site drops its target to pkey 0 so it can't
// call __ia2_parse_str directly. Instead we must define another wrapper
// __ia2_parse_str_0_2 which calls __ia2_parse_str. This manually defined
// wrapper expects a caller with pkey 0 and calls __ia2_parse_str with pkey 2.
// Putting it together we have:
// callsite wrapper (caller 1/target 0) calls
// __ia2_parse_str_0_2 (caller 0/target 2) which calls
// __ia2_parse_str (caller 2/target 1).

// TODO: When we switch to `ld --wrap` (#98) we'll be able to reference
// the real parse_str from here and this will only require two wrappers instead
// of three.
IA2_DEFINE_WRAPPER(parse_str, _ZTSPFvPcPvE, 2);
// See comment on parse_str
IA2_DEFINE_WRAPPER(parse_u32, _ZTSPFvPcPvE, 2);
// See comment on parse_str
IA2_DEFINE_WRAPPER(parse_bool, _ZTSPFvPcPvE, 2);

// IA2_SHARED_DATA makes all values in this struct shared, which includes the
// `char *` but not the string literals. This means that the other compartment
// can see where the string literals are but will not be able to read them. This
// is fine since the other compartment doesn't need to read them.
static struct cfg_opt opts[6] IA2_SHARED_DATA = {
    {
        "name",
        str,
        IA2_WRAPPER(parse_str, 2),
    },
    {
        "num_options",
        u32,
        IA2_WRAPPER(parse_u32, 2),
    },
    {
        "debug_mode",
        boolean,
        IA2_WRAPPER(parse_bool, 2),
    },
    {
        "magic_val",
        other,
        IA2_WRAPPER(parse_tuple, 2),
    },
    {
        "some_flag",
        boolean,
        IA2_WRAPPER(parse_bool, 2),
    },
    {
        "random_seed",
        u32,
        IA2_WRAPPER(parse_u32, 2),
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
  printf("Option %s not found!", name);
  exit(-1);
}

void print_tuple(struct tuple *tup) {
  // These derefs are fine since the heap is shared.
  printf("(%x, %x)\n", tup->first, tup->second);
}
