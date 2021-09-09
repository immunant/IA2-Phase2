#pragma once

typedef void (*HookFn)(void);

extern void set_exit_hook(HookFn);
