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

struct Op  get_fn(void) {
    if (incr) {
        return (struct Op){
            .op = &increment
        };
    }
    return (struct Op){
        .op = &decrement
    };
}
