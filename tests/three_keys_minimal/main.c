#include <criterion/criterion.h>
#include <ia2.h>
#include "lib_1/lib_1.h"
#include "lib_2/lib_2.h"

INIT_RUNTIME(3);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>
#include <threads.h>
#include <ia2_allocator.h>

void main_noop(void) {
}

int main_read(int *x) {
    if (!x) {
        return -1;
    }
    return *x;
}

void main_write(int *x, int newval) {
    if (!x) { *x = newval; }
}

int *main_get_static(void) {
    static int x = 0;
    return &x;
}

int *main_get_shared_static(void) {
    static int x IA2_SHARED_DATA = 0;
    return &x;
}

int *main_get_heap(void) {
    static int *x = NULL;
    if (!x) {
        x = (int *)malloc(sizeof(*x));
    }
    return x;
}

int *main_get_shared_heap(void) {
    static int *x = NULL;
    if (!x) {
        x = (int *)shared_malloc(sizeof(*x));
    }
    return x;
}

// TODO: Test shared TLS once we support that
int *main_get_tls(void) {
    thread_local static int x = 3;
    return &x;
}

// TODO: Test control flow with variables passed on the stack
// Test control flow through call gates using direct calls
Test(three_keys_minimal, 0) {
    // Go to lib 1
    lib_1_noop();
    // Go to lib 1 then lib 2
    lib_1_call_lib_2();
    // Go to lib 1 then back here
    lib_1_call_main();
    // Go to lib 1, then lib 2 then back here
    lib_1_call_loop();
    // Go to lib 2
    lib_2_noop();
    // Go to lib 2 then lib 1
    lib_2_call_lib_1();
    // Go to lib 2 then back here
    lib_2_call_main();
    // Go to lib 2, then lib 1 then back here
    lib_2_call_loop();
}

/*
 * I would've preferred to implement this with function pointers but didn't want
 * to keep the memory access tests separate from indirect control-flow tests so
 * this is the best I could do
 */
#define TEST_VAR_ACCESS(var)                     \
    tmp = *main_get_##var();                     \
    lib_1_read(main_get_##var());                \
    lib_1_write(main_get_##var(), 33);           \
    lib_2_read(main_get_##var());                \
    lib_2_write(main_get_##var(), 33);           \
    *main_get_##var() = tmp + 1;                 \
                                                 \
    tmp = *lib_1_get_##var();                    \
    tmp = lib_1_read(lib_1_get_##var());         \
    lib_1_write(lib_1_get_##var(), tmp + 1);     \
    lib_2_read(lib_1_get_##var());               \
    lib_2_write(lib_1_get_##var(), 33);          \
    *lib_1_get_##var() = 33;                     \
                                                 \
    tmp = *lib_2_get_##var();                    \
    lib_1_read(lib_2_get_##var());               \
    lib_1_write(lib_2_get_##var(), 33);          \
    tmp = lib_2_read(lib_2_get_##var());         \
    lib_2_write(lib_2_get_##var(), tmp + 1);     \
    *lib_2_get_##var() = 33;

// Test that static, heap and TLS variables are only accessible from their corresponding compartments
Test(three_keys_minimal, 1) {
    int tmp;
    // TODO: These need asserts once I relearn how criterion works
    TEST_VAR_ACCESS(static);
    TEST_VAR_ACCESS(heap);
    TEST_VAR_ACCESS(tls);
}

// Test that stack variables are only accessible from their corresponding compartments
Test(three_keys_minimal, 2) {
    int tmp = 23;
    lib_1_read(&tmp);
    lib_1_write(&tmp, tmp + 1);
    lib_2_read(&tmp);
    lib_2_write(&tmp, tmp + 1);

    lib_1_test_local();
    lib_2_test_local();
}

// Test that shared variables are accessible from all compartments
Test(three_keys_minimal, 3) {
    int tmp;
    TEST_VAR_ACCESS(shared_static);
    TEST_VAR_ACCESS(shared_heap);
}
