
// Check that readelf shows exactly one executable segment
#include "plugin.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <ia2.h>
#include <ia2_allocator.h>
#include <string.h>
// TODO: Add the `#include output_header.h` to shared headers since they may
// need wrapped function pointer definitions. For now just hack around this by
// including plugin.h (which does include the output header) before core.h.
#include "core.h"
#define IA2_DEFINE_TEST_HANDLER
#include <test_fault_handler.h>

/*
    This test is modeled after nginx's function pointer usage. In this test,
    plugins define a list of options that may appear in its section of the
    config file and describe how to parse each option. The main binary provides
    some functions for parsing common primitive types and allows the plugin to
    create parsing functions for custom data types. There is also a built-in
    module which is part of the main binary but uses the same indirect callsite
    as the plugin.
*/

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

// The first 5 entries are plugin options and the rest are builtin
#define PLUGIN_ENTRIES 5
#define BUILTIN_ENTRIES 3

// A config file with one entry per line. The entry name is separated from its
// value by '=' and the entry type is declared by the corresponding cfg_opt in
// the plugin.
const char *config_file =
    "name=config1\n\
num_options=4\n\
debug_mode=false\n\
magic_val=\xef\xbe\xad\xde\xaa\xbb\xcc\xdd\n\
random_seed=42\n\
name=builtin_config\n\
num_options=3\n\
array=\x11\x22\x33";

void parse_bool(char *opt, void *out) {
  bool *res = out;
  *res = strcmp(opt, "false");
}

void parse_str(char *opt, void *out) {
  char **res = out;
  strcpy(*res, opt);
}

void parse_u32(char *opt, void *out) {
  uint32_t *res = out;
  *res = strtol(opt, NULL, 10);
}

Test(read_config, main) {
  // Pretend that the heap is intentionally shared for now
  char *cfg = (char *)shared_malloc(strlen(config_file));
  if (!cfg) {
    cr_log_error("Could not perform shared malloc");
    exit(EXIT_FAILURE);
  }
  strcpy(cfg, config_file);

  char *tok = strtok(cfg, "\n");

  struct cfg_entry entries[8];
  size_t idx = 0;

  while (tok) {
    char *delim = strchr(tok, '=');
    *delim = '\0';

    entries[idx].name = (char *)shared_malloc(strlen(tok));
    strcpy(entries[idx].name, tok);

    struct cfg_opt *opt;
    if (idx < PLUGIN_ENTRIES) {
      // This passes a pointer to the heap to the other compartment which is
      // fine since we're pretending the heap is intentionally shared for now
      opt = get_opt(tok);
    } else {
      opt = get_builtin_opt(tok);
    }

    // `opt` may point to other compartment, but this dereference is fine since
    // the plugin has its options marked as shared data
    entries[idx].ty = opt->ty;

    // The `entries[idx].value` which we want to fill out by calling
    // `opt->parse` is on this compartment's stack. Since `opt->parse` may be a
    // function in the plugin, we need to use a shared buffer. We only need a
    // shared buffer for the entry's `value` field but that type is anonymously
    // defined within the cfg_entry struct so let's just make a whole `struct
    // cfg_entry` since it's small.
    static struct cfg_entry shared_entry IA2_SHARED_DATA;

    // These derefs are fine since the plugin sent a pointer to shared data.
    if (opt->ty == str) {
      // Depending on the option's type, we may need to allocate space for a
      // string
      shared_entry.value.str = (char *)shared_malloc(strlen(delim + 1));
    } else if (opt->ty == other) {
      shared_entry.value.other = shared_malloc(strlen(delim + 1));
    }

    // This function pointer may come from the plugin, so drop from pkey 1 to
    // pkey 0 before calling it. If the function is in the built-in module,
    // it'll have a wrapper from pkey 0 to pkey 1.
    (opt->parse)(delim + 1, &shared_entry.value);
    // Copy the value from the shared entry to the main binary's stack.
    entries[idx].value = shared_entry.value;
    idx++;

    tok = strtok(NULL, "\n");
  }

  for (size_t i = 0; i < PLUGIN_ENTRIES + BUILTIN_ENTRIES; i++) {
    cr_log_info("%s ", entries[i].name);
    switch (entries[i].ty) {
    case str: {
      cr_log_info("%s", entries[i].value.str);
      break;
    }
    case boolean: {
      if (entries[i].value.boolean) {
        cr_log_info("true");
      } else {
        cr_log_info("false");
      }
      break;
    }
    case u32: {
      cr_log_info("%d", entries[i].value.integer);
      break;
    }
    case other: {
      cr_log_info("at %p ", entries[i].value.other);
      if (i < PLUGIN_ENTRIES) {
        // Passing this pointer to the plugin is fine since we allocate via
        // shared_malloc.
        print_tuple(entries[i].value.other);
      } else {
        // This passes a pointer to the builtin module, so it's not an issue.
        print_array(entries[i].value.other);
      }
      break;
    }
    }
  }

  // We fault in a destructor while exiting, TODO: #112
  expect_fault = true;
}
