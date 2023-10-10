#pragma once
#include <stdint.h>
#include <stdbool.h>
 
// The main binary provides function for parsing basic config entries. To parse
// other data types the plugin must provide a parsing function.
void parse_bool(char *opt, void *out);

void parse_str(char *opt, void *out);

void parse_u32(char *opt, void *out);

bool register_plugin(unsigned int idx);

enum entry_type {
    str,
    boolean,
    u32,
    other,
};

// Functions that parse config entries must have this signature.
typedef void(*parse_fn)(char *, void *);

// The config options the plugin expects and a function pointer to their parsing
// functions.
struct cfg_opt {
    char *name;
    enum entry_type ty;
    parse_fn parse;
};

// The entry in the config file.
struct cfg_entry {
    char *name;
    enum entry_type ty;
    union {
        char *str;
        bool boolean;
        uint32_t integer;
        void *other;
    } value;
};
