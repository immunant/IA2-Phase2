#include <stdio.h>

extern void (*__start_fake_criterion_tests)(void);
extern void (*__stop_fake_criterion_tests)(void);

int main() {
    void (**test)(void) = &__start_fake_criterion_tests;
    for (; test < &__stop_fake_criterion_tests; test++) {
        if (!test) {
            break;
        }
        (*test)();
    }
}
