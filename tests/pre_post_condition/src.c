#include "src.h"
#include <ia2_test_runner.h>
#include <signal.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

void f1(int a, int *b) {
  *b = a;
}

IA2_PRE_CONDITION_FOR(f1)
void a_positive(int a, int *b) {
  if (!(a > 0)) {
    exit(10);
  }
}

IA2_PRE_CONDITION_FOR(f1)
void b_non_null(int a, int *b) {
  if (!b) {
    exit(11);
  }
}

IA2_PRE_CONDITION_FOR(f1)
void b_eq_0(int a, int *b) {
  if (!b) {
    return;
  }
  if (!(*b == 0)) {
    exit(13);
  }
}

IA2_POST_CONDITION_FOR(f1)
void a_eq_b(int a, int *b) {
  if (!(a == *b)) {
    exit(20);
  }
}

IA2_POST_CONDITION_FOR(f1)
void b_eq_10(int a, int *b) {
  if (!(*b == 10)) {
    exit(21);
  }
}

IA2_PRE_CONDITION_FOR(f2)
void exit_30(void) {
  exit(30);
}

IA2_POST_CONDITION_FOR(f2)
void exit_31(void) {
  exit(31);
}
