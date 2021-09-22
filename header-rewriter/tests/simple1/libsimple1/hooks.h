#pragma once

typedef void (*HookFn)(void);

extern HookFn get_exit_hook(void);
extern void set_exit_hook(HookFn);
