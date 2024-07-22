#include <stdio.h>
#include <criterion/new/assert.h>

#define cr_log_info(f, ...) printf(f "\n", ##__VA_ARGS__)
#define cr_log_error(f, ...) fprintf(stderr, f "\n", ##__VA_ARGS__)
