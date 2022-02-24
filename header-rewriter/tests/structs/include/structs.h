/*
RUN: cp %s %t.h
RUN: ia2-header-rewriter %T/wrapper.c %t.h -- -I%resource_dir
RUN: cat %t.h | sed 's/^.*CHECK.*$//' | FileCheck %s
RUN: %binary_dir/tests/structs/structs-main | diff %binary_dir/tests/structs/structs.out -
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct s1 {
	int i1;
	int i2;
};

struct s2 {
	float f1;
};

struct s3 {
    int i1;
    char c1;
    unsigned int u1;
};

struct s4 {
	float f1;
	float f2;
};

struct s5 {
	int i1;
	void* p1;
	char ac1[3];
	void* p2;
	float f1;
};

struct __attribute__((packed)) s6 {
	int i1;
	char c1;
	int i2;
	char c2;
	size_t z1;
};

struct s7 {
	bool b1 : 1;
	char c1 : 7;
	unsigned int u1;
};

struct s8 {
	bool b1 : 1;
	char c1 : 5;
	size_t z1 : 54;
	char c2 : 7;
	size_t z2 : 61;
	size_t z3;
};

struct s9 {
	__int128 i1 : 54;
	__int128 i2 : 13;
	__int128 i3 : 61;
};

struct s10 {
	__int128 i1 : 54;
	__int128 i2 : 13;
	__int128 i3 : 61;
	size_t z1;
};

struct s11 {
	size_t az1[9];
};

struct s12 {
	char ac1[9];
};

struct s13 {
    __int128 x;
};

// CHECK: IA2_WRAP_FUNCTION(get_s1);
struct s1 get_s1(void);

// CHECK: IA2_WRAP_FUNCTION(cksum_s1);
int cksum_s1(struct s1);

// CHECK: IA2_WRAP_FUNCTION(get_s2);
struct s2 get_s2(void);

// CHECK: IA2_WRAP_FUNCTION(extract_s2);
float extract_s2(struct s2);

// CHECK: IA2_WRAP_FUNCTION(get_s3);
struct s3 get_s3(void);

// CHECK: IA2_WRAP_FUNCTION(cksum_s3);
int cksum_s3(struct s3);

// CHECK: IA2_WRAP_FUNCTION(inc_s3);
struct s3 inc_s3(struct s3);

// CHECK: IA2_WRAP_FUNCTION(get_s4);
struct s4 get_s4(void);

// CHECK: IA2_WRAP_FUNCTION(cksum_s4);
float cksum_s4(struct s4);

// CHECK: IA2_WRAP_FUNCTION(get_s5);
struct s5 get_s5(void);

// CHECK: IA2_WRAP_FUNCTION(get_s5_int);
struct s5 get_s5_int(int);

// CHECK: IA2_WRAP_FUNCTION(print_s5);
void print_s5(struct s5 );

// CHECK: IA2_WRAP_FUNCTION(cksum_s5_f);
float cksum_s5_f(struct s5);

// CHECK: IA2_WRAP_FUNCTION(cksum_s5_z);
size_t cksum_s5_z(struct s5);

// CHECK: IA2_WRAP_FUNCTION(get_s6);
struct s6 get_s6(void);

// CHECK: IA2_WRAP_FUNCTION(cksum_s6);
size_t cksum_s6(struct s6);

// CHECK: IA2_WRAP_FUNCTION(inc_s6);
struct s6 inc_s6(struct s6);

// CHECK: IA2_WRAP_FUNCTION(mix_s6);
struct s6 mix_s6(struct s6 s1, struct s6 s2);

// CHECK: IA2_WRAP_FUNCTION(get_s7);
struct s7 get_s7(void);

// CHECK: IA2_WRAP_FUNCTION(cksum_s7);
unsigned int cksum_s7(struct s7);

// CHECK: IA2_WRAP_FUNCTION(get_s8);
struct s8 get_s8(void);

// CHECK: IA2_WRAP_FUNCTION(cksum_s8);
size_t cksum_s8(struct s8);

// CHECK: IA2_WRAP_FUNCTION(get_s9);
struct s9 get_s9(void);

// CHECK: IA2_WRAP_FUNCTION(cksum_s9);
__int128 cksum_s9(struct s9);

// CHECK: IA2_WRAP_FUNCTION(get_s10);
struct s10 get_s10(void);

// CHECK: IA2_WRAP_FUNCTION(cksum_s10);
__int128 cksum_s10(struct s10);

// CHECK: IA2_WRAP_FUNCTION(get_s11);
struct s11 get_s11(void);

// CHECK: IA2_WRAP_FUNCTION(cksum_s11);
unsigned int cksum_s11(struct s11);

// CHECK: IA2_WRAP_FUNCTION(get_s12);
struct s12 get_s12(void);

// CHECK: IA2_WRAP_FUNCTION(cksum_s12);
unsigned int cksum_s12(struct s12);

// CHECK: IA2_WRAP_FUNCTION(get_s13);
struct s13 get_s13(void);

// CHECK: IA2_WRAP_FUNCTION(cksum_s13);
unsigned int cksum_s13(struct s13);
