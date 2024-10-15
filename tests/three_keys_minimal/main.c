#include "ia2_internal.h"
#include "lib_1/lib_1.h"
#include "lib_2/lib_2.h"
#include <ia2_test_runner.h>
#include <ia2.h>

INIT_RUNTIME(3);
#define IA2_COMPARTMENT 1
#include <ia2_allocator.h>
#include <ia2_compartment_init.inc>
#include <threads.h>

#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

void main_noop(void) {
}

int main_read(int *x) {
  if (!x) {
    return -1;
  }
  return *x;
}

void main_write(int *x, int newval) {
  if (!x) {
    *x = newval;
  }
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
Test(three_keys_minimal, direct_calls) {
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

// variant of CHECK_VIOLATION macro that works with void-valued expressions
#define CHECK_VIOLATION_VOID(expr) \
  {                                \
    expect_fault = true;           \
    expr;                          \
    expect_fault = false;          \
  }

/*
 * I would've preferred to implement this with function pointers but didn't want
 * to keep the memory access tests separate from indirect control-flow tests so
 * this is the best I could do
 */
#define TEST_VAR_ACCESS(var, cond)                            \
  tmp = *main_get_##var();                                    \
  if (cond)                                                   \
    CHECK_VIOLATION(lib_1_read(main_get_##var()));            \
  if (cond)                                                   \
    CHECK_VIOLATION_VOID(lib_1_write(main_get_##var(), 33));  \
  if (cond)                                                   \
    CHECK_VIOLATION(lib_2_read(main_get_##var()));            \
  if (cond)                                                   \
    CHECK_VIOLATION_VOID(lib_2_write(main_get_##var(), 33));  \
  *main_get_##var() = tmp + 1;                                \
                                                              \
  if (cond)                                                   \
    tmp = CHECK_VIOLATION(*lib_1_get_##var());                \
  tmp = lib_1_read(lib_1_get_##var());                        \
  lib_1_write(lib_1_get_##var(), tmp + 1);                    \
  if (cond)                                                   \
    CHECK_VIOLATION(lib_2_read(lib_1_get_##var()));           \
  if (cond)                                                   \
    CHECK_VIOLATION_VOID(lib_2_write(lib_1_get_##var(), 33)); \
  if (cond)                                                   \
    CHECK_VIOLATION(*lib_1_get_##var() = 33);                 \
                                                              \
  if (cond)                                                   \
    tmp = CHECK_VIOLATION(*lib_2_get_##var());                \
  if (cond)                                                   \
    CHECK_VIOLATION(lib_1_read(lib_2_get_##var()));           \
  if (cond)                                                   \
    CHECK_VIOLATION_VOID(lib_1_write(lib_2_get_##var(), 33)); \
  tmp = lib_2_read(lib_2_get_##var());                        \
  lib_2_write(lib_2_get_##var(), tmp + 1);                    \
  if (cond)                                                   \
    CHECK_VIOLATION(*lib_2_get_##var() = 33);

// Test that static, heap and TLS variables are only accessible from their corresponding compartments
#define DECLARE_STATIC_TEST(n)                      \
  Test(three_keys_minimal, var_access_static_##n) { \
    int tmp;                                        \
    int check = n;                                  \
    TEST_VAR_ACCESS(static, check-- == 0)           \
  }
#define DECLARE_HEAP_TEST(n)                      \
  Test(three_keys_minimal, var_access_heap_##n) { \
    int tmp;                                      \
    int check = n;                                \
    TEST_VAR_ACCESS(heap, check-- == 0)           \
  }
#define DECLARE_TLS_TEST(n)                      \
  Test(three_keys_minimal, var_access_tls_##n) { \
    int tmp;                                     \
    int check = n;                               \
    TEST_VAR_ACCESS(tls, check-- == 0)           \
  }

REPEATB(11, DECLARE_STATIC_TEST, DECLARE_STATIC_TEST)
REPEATB(11, DECLARE_HEAP_TEST, DECLARE_HEAP_TEST)
REPEATB(11, DECLARE_TLS_TEST, DECLARE_TLS_TEST)

// Test that stack variables are only accessible from their corresponding compartments
void test_stack_vars(int check) {
  int tmp = 23;
  if (check-- == 0)
    CHECK_VIOLATION(lib_1_read(&tmp));
  if (check-- == 0)
    CHECK_VIOLATION_VOID(lib_1_write(&tmp, tmp + 1));
  if (check-- == 0)
    CHECK_VIOLATION(lib_2_read(&tmp));
  if (check-- == 0)
    CHECK_VIOLATION_VOID(lib_2_write(&tmp, tmp + 1));
  if (check-- == 0)
    CHECK_VIOLATION_VOID(lib_1_test_local());
  if (check-- == 0)
    CHECK_VIOLATION_VOID(lib_2_test_local());
}

#define DECLARE_STACK_TEST(n) \
  Test(three_keys_minimal, stack_vars_protected_##n) { test_stack_vars(n); }

REPEATB(5, DECLARE_STACK_TEST, DECLARE_STACK_TEST)

// Test that shared variables are accessible from all compartments
Test(three_keys_minimal, shared_vars_accessible) {
  int tmp;
  int check = -1;
#undef CHECK_VIOLATION
#define CHECK_VIOLATION(x) x
#undef CHECK_VIOLATION_VOID
#define CHECK_VIOLATION_VOID(x) x
  TEST_VAR_ACCESS(shared_static, true);
  TEST_VAR_ACCESS(shared_heap, true);
}
