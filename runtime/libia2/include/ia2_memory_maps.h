#pragma once

#include <stdio.h>

/// Log the labeled memory maps from `/proc/self/maps` to `log`.
/// This can be called at any point, including in a debugger.
void ia2_log_memory_maps(FILE *log);
