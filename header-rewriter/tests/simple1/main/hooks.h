#pragma once

#ifndef PREBUILT_LIB
#include "hook_ty.h"
#endif

HookFn get_exit_hook(void);
void set_exit_hook(HookFn);
