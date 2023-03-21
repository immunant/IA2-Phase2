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

struct s14 {
	unsigned int u1 : 1;
	unsigned int u2 : 1;
	unsigned int u3 : 1;
	unsigned int u4 : 1;
	unsigned int u5 : 1;
	unsigned int u6 : 1;
	unsigned int u7 : 1;
	unsigned int u8 : 1;
	unsigned int u9 : 1;
	unsigned int u10 : 1;
	unsigned int u11 : 1;
	unsigned int u12 : 1;
	unsigned int u13 : 1;
	unsigned int u14 : 1;
	unsigned int u15 : 1;
	unsigned int u16 : 1;
	unsigned int u17 : 1;
	unsigned int u18 : 1;
	unsigned int u19 : 1;
	unsigned int u20 : 1;
	unsigned int u21 : 1;
	unsigned int u22 : 1;
	unsigned int u23 : 1;
	unsigned int u24 : 1;
	unsigned int u25 : 1;
	unsigned int u26 : 1;
	unsigned int u27 : 1;
	unsigned int u28 : 1;
	unsigned int u29 : 1;
	unsigned int u30 : 1;
	unsigned int u31 : 1;
	unsigned int u32 : 1;
	unsigned int field2;
	unsigned int field3;
	unsigned int field4;
	unsigned int field5;
	unsigned int field6;
	unsigned int field7;
	unsigned int field8;
};

// LINKARGS: --wrap=get_s1
struct s1 get_s1(void);

// LINKARGS: --wrap=cksum_s1
int cksum_s1(struct s1);

// LINKARGS: --wrap=get_s2
struct s2 get_s2(void);

// LINKARGS: --wrap=extract_s2
float extract_s2(struct s2);

// LINKARGS: --wrap=get_s3
struct s3 get_s3(void);

// LINKARGS: --wrap=cksum_s3
int cksum_s3(struct s3);

// LINKARGS: --wrap=inc_s3
struct s3 inc_s3(struct s3);

// LINKARGS: --wrap=get_s4
struct s4 get_s4(void);

// LINKARGS: --wrap=cksum_s4
float cksum_s4(struct s4);

// LINKARGS: --wrap=get_s5
struct s5 get_s5(void);

// LINKARGS: --wrap=get_s5_int
struct s5 get_s5_int(int);

// LINKARGS: --wrap=print_s5
void print_s5(struct s5 );

// LINKARGS: --wrap=cksum_s5_f
float cksum_s5_f(struct s5);

// LINKARGS: --wrap=cksum_s5_z
size_t cksum_s5_z(struct s5);

// LINKARGS: --wrap=get_s6
struct s6 get_s6(void);

// LINKARGS: --wrap=cksum_s6
size_t cksum_s6(struct s6);

// LINKARGS: --wrap=inc_s6
struct s6 inc_s6(struct s6);

// LINKARGS: --wrap=mix_s6
struct s6 mix_s6(struct s6 s1, struct s6 s2);

// LINKARGS: --wrap=get_s7
struct s7 get_s7(void);

// LINKARGS: --wrap=cksum_s7
unsigned int cksum_s7(struct s7);

// LINKARGS: --wrap=get_s8
struct s8 get_s8(void);

// LINKARGS: --wrap=cksum_s8
size_t cksum_s8(struct s8);

// LINKARGS: --wrap=get_s9
struct s9 get_s9(void);

// LINKARGS: --wrap=cksum_s9
__int128 cksum_s9(struct s9);

// LINKARGS: --wrap=get_s10
struct s10 get_s10(void);

// LINKARGS: --wrap=cksum_s10
__int128 cksum_s10(struct s10);

// LINKARGS: --wrap=get_s11
struct s11 get_s11(void);

// LINKARGS: --wrap=cksum_s11
unsigned int cksum_s11(struct s11);

// LINKARGS: --wrap=get_s12
struct s12 get_s12(void);

// LINKARGS: --wrap=cksum_s12
unsigned int cksum_s12(struct s12);

// LINKARGS: --wrap=get_s13
struct s13 get_s13(void);

// LINKARGS: --wrap=cksum_s13
unsigned int cksum_s13(struct s13);

// LINKARGS: --wrap=get_s14
struct s14 get_s14(void);

// LINKARGS: --wrap=cksum_s14
unsigned int cksum_s14(struct s14);
