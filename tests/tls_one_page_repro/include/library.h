#pragma once

#include <stdint.h>

uint32_t lib2_call_shared_tls_bump(void);
uintptr_t lib2_get_shared_tls_addr(void);
uintptr_t lib2_get_local_tls_addr(void);
