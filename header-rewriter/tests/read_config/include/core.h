/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %T/wrapper.c.args | FileCheck --check-prefix=LINKARGS %s
TODO: %binary_dir/tests/read_config/read_config-main | diff %S/../Output/read_config.out -
RUN: readelf -lW %binary_dir/tests/read_config/read_config-main | FileCheck --check-prefix=SEGMENTS %s
RUN: readelf -lW %binary_dir/tests/read_config/libread_config-original.so | FileCheck --check-prefix=SEGMENTS %s
*/

// Check that readelf shows exactly one executable segment
// SEGMENTS-COUNT-1: LOAD{{.*}}R E
// SEGMENTS-NOT:     LOAD{{.*}}R E
#pragma once
#include <stdint.h>
#include <stdbool.h>

// Functions that parse config entries must have this signature.
typedef void(*parse_fn)(char *, void *);
 
// The main binary provides function for parsing basic config entries. To parse
// other data types the plugin must provide a parsing function.
// LINKARGS: --wrap=parse_bool
void parse_bool(char *opt, void *out);

// LINKARGS: --wrap=parse_str
void parse_str(char *opt, void *out);

// LINKARGS: --wrap=parse_u32
void parse_u32(char *opt, void *out);

enum entry_type {
    str,
    boolean,
    u32,
    other,
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

// The config options the plugin expects and a function pointer to their parsing
// functions.
struct cfg_opt {
    char *name;
    enum entry_type ty;
    parse_fn parse;
};

struct cfg_opt *get_builtin_opt(char *name);
void print_array(uint8_t ar[3]);
