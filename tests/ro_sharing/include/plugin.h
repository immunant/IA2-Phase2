#pragma once

#include <stdbool.h>
#include <stdint.h>

const char *get_plugin_str(void);
const uint32_t *get_plugin_uint(bool secret);
void read_main_string(const char *str);
void read_main_uint(const uint32_t *shared, const uint32_t *secret);
