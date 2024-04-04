#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

extern int (*__start_fake_criterion_tests)(void);
extern int (*__stop_fake_criterion_tests)(void);

int main() {
    int (**test)(void) = &__start_fake_criterion_tests;
    for (; test < &__stop_fake_criterion_tests; test++) {
        if (!test) {
            break;
        }
        pid_t pid = fork();
        bool in_child = pid == 0;
        if (in_child) {
            return (*test)();
        }
    }
}
