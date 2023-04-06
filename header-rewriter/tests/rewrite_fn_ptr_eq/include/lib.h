#pragma once

typedef int (*bin_op)(int, int);

int call_fn(bin_op fn, int x, int y);
