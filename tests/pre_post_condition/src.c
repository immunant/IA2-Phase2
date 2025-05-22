#include "src.h"
#include <signal.h>
#include <stdio.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

void f1(int a) {
  printf("%d\n", a);
}

void f2(int a, int *b) {
  *b = a;
}

void f3(int *a, int b, int c) {
  *a = b + c;
}

IA2_PRE_CONDITION_FOR(f2)
void a_positive(int a, int *b) {
  if (!(a > 0)) {
    exit(10);
  }
}

IA2_PRE_CONDITION_FOR(f2)
void b_non_null(int a, int *b) {
  if (!b) {
    exit(11);
  }
}

IA2_PRE_CONDITION_FOR(f2)
void b_eq_0(int a, int *b) {
  if (!b) {
    return;
  }
  if (!(*b == 0)) {
    exit(13);
  }
}

IA2_POST_CONDITION_FOR(f2)
void a_eq_b(int a, int *b) {
  if (!(a == *b)) {
    exit(20);
  }
}

IA2_POST_CONDITION_FOR(f2)
void b_eq_10(int a, int *b) {
  if (!(*b == 10)) {
    exit(21);
  }
}

IA2_PRE_CONDITION_FOR(f0)
void exit_30(void) {
  exit(30);
}

IA2_POST_CONDITION_FOR(f0)
void exit_31(void) {
  exit(31);
}

IA2_PRE_CONDITION_FOR(f1)
IA2_POST_CONDITION_FOR(f1)
void a_eq_11(int a) {
  if (!(a == 11)) {
    exit(40);
  }
}

IA2_PRE_CONDITION_FOR(f3)
void b_c_eq_11_22(int *a, int b, int c) {
  if (!(b == 11)) {
    exit(50);
  }
  if (!(c == 22)) {
    exit(51);
  }
}

IA2_POST_CONDITION_FOR(f3)
void a_eq_33(int *a, int b, int c) {
  if (!(*a == 33)) {
    exit(52);
  }
}
