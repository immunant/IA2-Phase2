#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

bool get_inferior_pkru(pid_t pid, uint32_t *pkru_out);
