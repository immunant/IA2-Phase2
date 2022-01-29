#pragma once

// The main binary requires the opaque pointer typedef because we are manually
// wrapping pointers in its source. Since libsimple1 makes direct calls to the
// main binary, we don't want the .symver statements on the hooks.h functions so
// this typedef is in a header by itself to simplify the `#include` graph
typedef void (*HookFn)(void);
