#pragma once

// This function does nothing, but should get wrapped
void foo();

// This function is not in the allowlist so it should not be wrapped
void defined_in_main();
