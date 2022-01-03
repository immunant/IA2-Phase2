#pragma once

typedef int (*F)(int);

struct Op {
    F op;
};

struct Op get_fn(void);

void change_fn();
