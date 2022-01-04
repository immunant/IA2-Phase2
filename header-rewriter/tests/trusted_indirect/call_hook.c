#include <stdbool.h>
#include "call_hook.h"

static bool incr = true;

static int increment(int x) {
    return x + 1;
}

static int decrement(int x) {
    return x - 1;
}

void change_fn() {
    incr = !incr;
}

// TODO: This only returns a struct instead of a function pointer because the
// rewriter currently doesn't support functions that return function pointers.
struct Op get_fn(void) {
    if (incr) {
        return (struct Op){
            .op = &increment
        };
    }
    return (struct Op){
        .op = &decrement
    };
}
