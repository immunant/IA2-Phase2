#pragma once

typedef void (*HookFn)(void);

HookFn get_exit_hook(void);
void set_exit_hook(HookFn);
